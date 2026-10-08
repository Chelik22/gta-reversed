#include "StdInc.h"

#include "rtquat.h"

RwBool RtQuatConvertFromMatrix(RtQuat* qpQuat, const RwMatrix* const mpMatrix) {
    if (qpQuat == NULL || mpMatrix == NULL) {
        return FALSE;
    }

    const RwReal trace = mpMatrix->at.z + mpMatrix->right.x + mpMatrix->up.y;

    if (0.0f /*0x858B50*/ < trace) {
        RwReal s = _rwSqrt(trace + 1.0f /*0x858624*/);
        qpQuat->real = 0.5f /*0x858B8C*/ * s;
        s = 0.5f /*0x858B8C*/ / s;
        qpQuat->imag.x = (mpMatrix->up.z - mpMatrix->at.y) * s;
        qpQuat->imag.y = (mpMatrix->at.x - mpMatrix->right.z) * s;
        qpQuat->imag.z = (mpMatrix->right.y - mpMatrix->up.x) * s;
        return TRUE;
    }

    if (mpMatrix->right.x <= mpMatrix->up.y) {
        // максимальна up.y или at.z
        if (mpMatrix->at.z < mpMatrix->up.y) {
            ((void(__cdecl*)(RtQuat*, const RwMatrix*))0x7EB700)(qpQuat, mpMatrix);  // ветка Y
        } else {
            ((void(__cdecl*)(RtQuat*, const RwMatrix*))0x7EB760)(qpQuat, mpMatrix);  // ветка Z
        }
    } else {
        // максимальна right.x или at.z
        if (mpMatrix->at.z < mpMatrix->right.x) {
            ((void(__cdecl*)(RtQuat*, const RwMatrix*))0x7EB6A0)(qpQuat, mpMatrix);  // ветка X
        } else {
            ((void(__cdecl*)(RtQuat*, const RwMatrix*))0x7EB760)(qpQuat, mpMatrix);  // ветка Z
        }
    }
    return TRUE;
}


RtQuat* RtQuatRotate(RtQuat* quat, const RwV3d* axis, RwReal angle, RwOpCombineType combineOp) {
    if (quat == NULL || axis == NULL) {
        RwError rwError;
        rwError.pluginID = 0x195;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000016);
        RwErrorSet(&rwError);
        return NULL;
    }

    const RwReal halfAngle = angle * 0.0087266462f /*0x8631D4 = pi/360*/;

    switch (combineOp) {
    case rwCOMBINEREPLACE:
    {
        const RwReal sinH = RwSin(halfAngle);
        const RwReal cosH = RwCos(halfAngle);
        quat->real = cosH;
        RwV3dNormalize(&quat->imag, axis);
        quat->imag.x = sinH * quat->imag.x;
        quat->imag.y = quat->imag.y * sinH;
        quat->imag.z = quat->imag.z * sinH;
        return quat;
    }
    case rwCOMBINEPRECONCAT:
    {
        // quat = quat * delta (Hamilton; librw mult(*this, rot))
        const RwV3d   q1imag = quat->imag;
        const RwReal  q1real = quat->real;
        const RwReal  sinH = RwSin(halfAngle);
        const RwReal  cosH = RwCos(halfAngle);
        RtQuat delta;
        RwV3dNormalize(&delta.imag, axis);
        delta.imag.x = sinH * delta.imag.x;
        delta.imag.y = delta.imag.y * sinH;
        delta.imag.z = delta.imag.z * sinH;
        const RwReal dReal = cosH;
        quat->real = dReal * q1real -
                     (delta.imag.z * q1imag.z + delta.imag.x * q1imag.x + delta.imag.y * q1imag.y);
        quat->imag.x = delta.imag.x * q1real + q1imag.x * dReal +
                       (q1imag.y * delta.imag.z - q1imag.z * delta.imag.y);
        quat->imag.y = delta.imag.y * q1real + q1imag.y * dReal +
                       (q1imag.z * delta.imag.x - delta.imag.z * q1imag.x);
        quat->imag.z = delta.imag.z * q1real + q1imag.z * dReal +
                       (delta.imag.y * q1imag.x - delta.imag.x * q1imag.y);
        return quat;
    }
    case rwCOMBINEPOSTCONCAT:
    {
        // quat = delta * quat (Hamilton; librw mult(rot, *this))
        const RwV3d   q1imag = quat->imag;
        const RwReal  q1real = quat->real;
        const RwReal  sinH = RwSin(halfAngle);
        const RwReal  cosH = RwCos(halfAngle);
        RtQuat delta;
        RwV3dNormalize(&delta.imag, axis);
        delta.imag.x = sinH * delta.imag.x;
        delta.imag.y = delta.imag.y * sinH;
        delta.imag.z = delta.imag.z * sinH;
        const RwReal dReal = cosH;
        quat->real = q1real * dReal -
                     (q1imag.z * delta.imag.z + q1imag.x * delta.imag.x + q1imag.y * delta.imag.y);
        quat->imag.x = q1imag.x * dReal + delta.imag.x * q1real +
                       (q1imag.z * delta.imag.y - delta.imag.z * q1imag.y);
        quat->imag.y = delta.imag.y * q1real + q1imag.y * dReal +
                       (q1imag.x * delta.imag.z - q1imag.z * delta.imag.x);
        quat->imag.z = q1imag.z * dReal + delta.imag.z * q1real +
                       (q1imag.y * delta.imag.x - delta.imag.y * q1imag.x);
        return quat;
    }
    default:
    {
        RwError rwError;
        rwError.pluginID = 0x195;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32, ...))0x8088D0)(0x80000003,
                                                                       "Invalid combination type");
        RwErrorSet(&rwError);
        return NULL;
    }
    }
}


const RtQuat* RtQuatQueryRotate(const RtQuat* quat, RwV3d* unitAxis, RwReal* angle) {
    if (quat == NULL || unitAxis == NULL || angle == NULL) {
        RwError rwError;
        rwError.pluginID = 0x195;
        rwError.errorCode = ((RwInt32(__cdecl*)(RwInt32))0x8088D0)(0x80000016);
        RwErrorSet(&rwError);
        return NULL;
    }

    const RwReal sinB = _rwSqrt(quat->imag.z * quat->imag.z +
                                quat->imag.y * quat->imag.y +
                                quat->imag.x * quat->imag.x);
    RwReal factor = 0.0f /*0x858B50*/;
    if (0.0f /*0x858B50*/ < sinB) {
        factor = 1.0f /*0x858624*/ / sinB;
    }

    const RwReal halfAngle = RwATan2(sinB, quat->real);

    if (halfAngle <= 1.5707964f /*0x858FE4 = pi/2*/) {
        unitAxis->x = factor * quat->imag.x;
        unitAxis->y = quat->imag.y * factor;
        unitAxis->z = quat->imag.z * factor;
        *angle = halfAngle * 114.591553f /*0x884410 = 360/pi*/;
        return quat;
    }

    factor = -factor;
    unitAxis->x = factor * quat->imag.x;
    unitAxis->y = quat->imag.y * factor;
    unitAxis->z = quat->imag.z * factor;
    *angle = (3.1415927f /*0x858CB8 = pi*/ - halfAngle) * 114.591553f /*0x884410 = 360/pi*/;
    return quat;
}


RwV3d* RtQuatTransformVectors(RwV3d* vectorsOut, const RwV3d* vectorsIn, const RwInt32 numPoints, const RtQuat* quat) {
    const RwReal wSqMinusISq = quat->real * quat->real -
                               (quat->imag.z * quat->imag.z +
                                quat->imag.y * quat->imag.y +
                                quat->imag.x * quat->imag.x);
    const RwReal twoX = quat->imag.x + quat->imag.x;
    const RwReal twoY = quat->imag.y + quat->imag.y;
    const RwReal twoZ = quat->imag.z + quat->imag.z;
    const RwReal twoWx = quat->real * twoX;
    const RwReal twoWy = quat->real * twoY;
    const RwReal twoWz = quat->real * twoZ;

    if (0 < numPoints) {
        RwV3d*       out = vectorsOut;
        const RwV3d* in  = vectorsIn;
        RwInt32      n   = numPoints;
        do {
            const RwReal dot = in->z * twoZ + twoX * in->x + in->y * twoY;
            out->x = in->z * twoWy - in->y * twoWz;
            out->y = twoWz * in->x - in->z * twoWx;
            out->z = in->y * twoWx - twoWy * in->x;
            out->x = dot * quat->imag.x + out->x;
            out->y = quat->imag.y * dot + out->y;
            out->z = quat->imag.z * dot + out->z;
            out->x = wSqMinusISq * in->x + out->x;
            out->y = in->y * wSqMinusISq + out->y;
            out->z = in->z * wSqMinusISq + out->z;
            ++in;
            ++out;
            --n;
        } while (n != 0);
    }
    return vectorsOut;
}


RwReal RtQuatModulus(RtQuat* q) {
    return _rwSqrt(q->real * q->real +
                   q->imag.z * q->imag.z +
                   q->imag.y * q->imag.y +
                   q->imag.x * q->imag.x);
}

