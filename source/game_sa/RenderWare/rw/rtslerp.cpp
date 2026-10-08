#include "StdInc.h"

#include "rtslerp.h"

void RtQuatSetupSlerpCache(RtQuat* qpFrom, RtQuat* qpTo, RtQuatSlerpCache* sCache)
{
    RwReal dot;
    RwReal omega;

    /* scaled initial quaternion */
    sCache->raFrom = *qpFrom;

    /* cosine of the angle between quaternions (operand order per decompile) */
    dot = qpFrom->real * qpTo->real +
          qpFrom->imag.z * qpTo->imag.z +
          qpFrom->imag.x * qpTo->imag.x +
          qpFrom->imag.y * qpTo->imag.y;

    if (dot >= 0.0f)
    {
        /* clamp to [0..1] (decompile: > 1.0f -> 1.0f) */
        if (1.0f < dot)
        {
            dot = 1.0f;
        }

        /* keep target as-is */
        sCache->raTo = *qpTo;
    }
    else
    {
        /* shortest path: negate the target quaternion */
        if (-1.0f <= dot)
        {
            dot = -dot;
        }
        else
        {
            dot = 1.0f;
        }

        sCache->raTo.real = -qpTo->real;
        sCache->raTo.imag.x = -qpTo->imag.x;
        sCache->raTo.imag.y = -qpTo->imag.y;
        sCache->raTo.imag.z = -qpTo->imag.z;
    }

    /*
     * Angular displacement in radians. The compiler inlined the CRT acos()
     * (polynomial coefficients at 0x874E64..0x874E90 verified in the binary:
     * pi, pi/2 and the asin/acos rational approximation); its special cases
     * are reproduced: acos(1) = 0, acos(-1) = pi (unreachable here after the
     * clamp), |dot| < ~6.9e-18 -> pi/2.
     */
    omega = (RwReal) acos(dot);

    sCache->omega = omega;

    /* nearly-zero / nearly-pi angle -> RtQuatSlerpMacro falls back to lerp
     * (decompile: nearlyZeroOm = (0.99999f <= dot), constant _DAT_00871258 = 0.99999f) */
    sCache->nearlyZeroOm = (RwBool) (0.99999f <= dot);

    if ((RwReal) sCache->nearlyZeroOm == 0.0f)
    {
        /* scale both cached quaternions by 1/sin(omega) — the compiler inlined
         * the CRT sin() series (1/6, 1/120, 1/5040, ... coefficients at
         * 0x85F0A0/0x864E0C/0x86D2A4..0x86D2B0 verified in the binary) */
        RwReal recipSin = 1.0f / (RwReal) sin(omega);

        /* decompile scaling order: real, imag.x, imag.y, imag.z
         * (operand order mirrored from decompile; FP multiply is commutative) */
        sCache->raFrom.real = recipSin * sCache->raFrom.real;
        sCache->raFrom.imag.x = recipSin * sCache->raFrom.imag.x;
        sCache->raFrom.imag.y = sCache->raFrom.imag.y * recipSin;
        sCache->raFrom.imag.z = sCache->raFrom.imag.z * recipSin;

        sCache->raTo.real = recipSin * sCache->raTo.real;
        sCache->raTo.imag.x = recipSin * sCache->raTo.imag.x;
        sCache->raTo.imag.y = sCache->raTo.imag.y * recipSin;
        sCache->raTo.imag.z = sCache->raTo.imag.z * recipSin;
    }
}

