/************************************************************************************

Filename    :   Util_MagCalibration.cpp
Content     :   Procedures for calibrating the magnetometer
Created     :   April 16, 2013
Authors     :   Steve LaValle, Andrew Reisse

Copyright   :   Copyright 2013 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#include "Util_MagCalibration.h"

namespace OVR { namespace Util {

void MagCalibration::BeginAutoCalibration(SensorFusion& sf)
{
    Status = Mag_AutoCalibrating;
    // This is a "hard" reset of the mag, so need to clear stored values
    sf.ClearMagCalibration();
    SampleCount = 0;
}

unsigned MagCalibration::UpdateAutoCalibration(SensorFusion& sf)
{
    if (Status != Mag_AutoCalibrating)
        return Status;

    Quatf q = sf.GetOrientation();
    Vector3f m = sf.GetMagnetometer();

    InsertIfAcceptable(q, m);

    if ((SampleCount == 4) && (Status == Mag_AutoCalibrating))
        SetCalibration(sf);

    return Status;

}

void MagCalibration::BeginManualCalibration(SensorFusion& sf)
{
    Status = Mag_ManuallyCalibrating;
    sf.ClearMagCalibration();
    SampleCount = 0;
}

bool MagCalibration::IsAcceptableSample(const Quatf& q, const Vector3f& m)
{
    switch (SampleCount)
    {
        // Initial sample is always acceptable
    case 0:
        return true;
        break;
    case 1:
        return (q.DistanceSq(QuatSamples[0]) > MinQuatDistanceSq)&&
               ((m - MagSamples[0]).LengthSq() > MinMagDistanceSq);
        break;
    case 2:
        return (q.DistanceSq(QuatSamples[0]) > MinQuatDistanceSq)&&
               (q.DistanceSq(QuatSamples[1]) > MinQuatDistanceSq)&&
               ((m - MagSamples[0]).LengthSq() > MinMagDistanceSq)&&
               ((m - MagSamples[1]).LengthSq() > MinMagDistanceSq);
        break;
    case 3:
        return (q.DistanceSq(QuatSamples[0]) > MinQuatDistanceSq)&&
               (q.DistanceSq(QuatSamples[1]) > MinQuatDistanceSq)&&
               (q.DistanceSq(QuatSamples[2]) > MinQuatDistanceSq)&&
               ((PointToPlaneDistance(MagSamples[0],MagSamples[1],MagSamples[2],m) > MinMagDistance)||
                (PointToPlaneDistance(MagSamples[1],MagSamples[2],m,MagSamples[0]) > MinMagDistance)||
                (PointToPlaneDistance(MagSamples[2],m,MagSamples[0],MagSamples[1]) > MinMagDistance)||
                (PointToPlaneDistance(m,MagSamples[0],MagSamples[1],MagSamples[2]) > MinMagDistance));
    }

    return false;
}


bool MagCalibration::InsertIfAcceptable(const Quatf& q, const Vector3f& m)
{
    if (IsAcceptableSample(q, m))
    {
        MagSamples[SampleCount] = m;
        QuatSamples[SampleCount] = q;
        SampleCount++;
        return true;
    }

    return false;
}


bool MagCalibration::SetCalibration(SensorFusion& sf)
{
    if (SampleCount < 4)
        return false;

    MagCenter = CalculateSphereCenter(MagSamples[0],MagSamples[1],MagSamples[2],MagSamples[3]);
    Matrix4f calMat = Matrix4f();
    calMat.M[0][3] = -MagCenter.x;
    calMat.M[1][3] = -MagCenter.y;
    calMat.M[2][3] = -MagCenter.z;
    sf.SetMagCalibration(calMat);
    Status = Mag_Calibrated;
    //LogText("MagCenter: %f %f %f\n",MagCenter.x,MagCenter.y,MagCenter.z);

    return true;
}


// Calculate the center of a sphere that passes through p1, p2, p3, p4
Vector3f MagCalibration::CalculateSphereCenter(const Vector3f& p1, const Vector3f& p2,
                                               const Vector3f& p3, const Vector3f& p4) 
{
    Matrix4f A;
    int i;
    Vector3f p[4];
    p[0] = p1;
    p[1] = p2;
    p[2] = p3;
    p[3] = p4;

    for (i = 0; i < 4; i++) 
    {
        A.M[i][0] = p[i].x;
        A.M[i][1] = p[i].y;
        A.M[i][2] = p[i].z;
        A.M[i][3] = 1.0f;
    }
    float m11 = A.Determinant();
    OVR_ASSERT(m11 != 0.0f);

    for (i = 0; i < 4; i++) 
    {
        A.M[i][0] = p[i].x*p[i].x + p[i].y*p[i].y + p[i].z*p[i].z;
        A.M[i][1] = p[i].y;
        A.M[i][2] = p[i].z;
        A.M[i][3] = 1.0f;
    }
    float m12 = A.Determinant();

    for (i = 0; i < 4; i++) 
    {
        A.M[i][0] = p[i].x*p[i].x + p[i].y*p[i].y + p[i].z*p[i].z;
        A.M[i][1] = p[i].x;
        A.M[i][2] = p[i].z;
        A.M[i][3] = 1.0f;
    }
    float m13 = A.Determinant();

    for (i = 0; i < 4; i++) 
    {
        A.M[i][0] = p[i].x*p[i].x + p[i].y*p[i].y + p[i].z*p[i].z;
        A.M[i][1] = p[i].x;
        A.M[i][2] = p[i].y;
        A.M[i][3] = 1.0f;
    }
    float m14 = A.Determinant();

    float c = 0.5f / m11;
    return Vector3f(c*m12, -c*m13, c*m14);
}

// Distance from p4 to the nearest point on a plane through p1, p2, p3
float MagCalibration::PointToPlaneDistance(const Vector3f& p1, const Vector3f& p2,
                                           const Vector3f& p3, const Vector3f& p4) 
{
    Vector3f v1 = p1 - p2;
    Vector3f v2 = p1 - p3;
    Vector3f planeNormal = v1.Cross(v2);
    planeNormal.Normalize();
    return (fabs((planeNormal * p4) - planeNormal * p1));
}

}}
