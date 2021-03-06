#ifndef pic_push_h
#define pic_push_h

#include <types.h>
#include "move_p.h"

void push(
        particle_list_t particles,
        interpolator_array_t& f0,
        real_t qdt_2mc,
        real_t cdt_dx,
        real_t cdt_dy,
        real_t cdt_dz,
        real_t qsp,
        accumulator_array_t& a0,
        grid_t* g,
        const size_t nx,
        const size_t ny,
        const size_t nz,
        const size_t num_ghosts,
        Boundary boundary
        )
{

    //auto _a = a0.slice<0>();

    auto slice = a0.slice<0>();
    decltype(slice)::atomic_access_slice _a = slice;

    auto position_x = particles.slice<PositionX>();
    auto position_y = particles.slice<PositionY>();
    auto position_z = particles.slice<PositionZ>();

    auto velocity_x = particles.slice<VelocityX>();
    auto velocity_y = particles.slice<VelocityY>();
    auto velocity_z = particles.slice<VelocityZ>();

    auto weight = particles.slice<Weight>();
    auto cell = particles.slice<Cell_Index>();

    //const real_t qdt_4mc        = -0.5*qdt_2mc; // For backward half rotate
    const real_t one            = 1.;
    const real_t one_third      = 1./3.;
    const real_t two_fifteenths = 2./15.;

    // We prefer making slices out side of the llambda
    auto _ex = f0.slice<EX>();
    auto _dexdy = f0.slice<DEXDY>();
    auto _dexdz = f0.slice<DEXDZ>();
    auto _d2exdydz = f0.slice<D2EXDYDZ>();
    auto _ey = f0.slice<EY>();
    auto _deydz = f0.slice<DEYDZ>();
    auto _deydx = f0.slice<DEYDX>();
    auto _d2eydzdx = f0.slice<D2EYDZDX>();
    auto _ez = f0.slice<EZ>();
    auto _dezdx = f0.slice<DEZDX>();
    auto _dezdy = f0.slice<DEZDY>();
    auto _d2ezdxdy = f0.slice<D2EZDXDY>();
    auto _cbx = f0.slice<CBX>();
    auto _dcbxdx = f0.slice<DCBXDX>();
    auto _cby = f0.slice<CBY>();
    auto _dcbydy = f0.slice<DCBYDY>();
    auto _cbz = f0.slice<CBZ>();
    auto _dcbzdz = f0.slice<DCBZDZ>();

    auto _push =
        KOKKOS_LAMBDA( const int s, const int i )
        {
            //for ( int i = 0; i < particle_list_t::vector_length; ++i )
            //{
            // Setup data accessors
            // This may be cleaner if we hoisted it?
            int ii = cell.access(s,i);

            auto ex = _ex(ii);
            auto dexdy = _dexdy(ii);
            auto dexdz = _dexdz(ii);
            auto d2exdydz = _d2exdydz(ii);
            auto ey = _ey(ii);
            auto deydz = _deydz(ii);
            auto deydx = _deydx(ii);
            auto d2eydzdx = _d2eydzdx(ii);
            auto ez = _ez(ii);
            auto dezdx = _dezdx(ii);
            auto dezdy = _dezdy(ii);
            auto d2ezdxdy = _d2ezdxdy(ii);
            auto cbx = _cbx(ii);
            auto dcbxdx = _dcbxdx(ii);
            auto cby = _cby(ii);
            auto dcbydy = _dcbydy(ii);
            auto cbz = _cbz(ii);
            auto dcbzdz = _dcbzdz(ii);
            /*
               auto ex  = f0.get<EX>(ii);
               auto dexdy  = f0.get<DEXDY>(ii);
               auto dexdz  = f0.get<DEXDZ>(ii);
               auto d2exdydz  = f0.get<D2EXDYDZ>(ii);
               auto ey  = f0.get<EY>(ii);
               auto deydz  = f0.get<DEYDZ>(ii);
               auto deydx  = f0.get<DEYDX>(ii);
               auto d2eydzdx  = f0.get<D2EYDZDX>(ii);
               auto ez  = f0.get<EZ>(ii);
               auto dezdx  = f0.get<DEZDX>(ii);
               auto dezdy  = f0.get<DEZDY>(ii);
               auto d2ezdxdy  = f0.get<D2EZDXDY>(ii);
               auto cbx  = f0.get<CBX>(ii);
               auto dcbxdx   = f0.get<DCBXDX>(ii);
               auto cby  = f0.get<CBY>(ii);
               auto dcbydy  = f0.get<DCBYDY>(ii);
               auto cbz  = f0.get<CBZ>(ii);
               auto dcbzdz  = f0.get<DCBZDZ>(ii);
               */

            // Perform push

            // TODO: deal with pm's
            particle_mover_t local_pm = particle_mover_t();

            real_t dx = position_x.access(s,i);   // Load position
            real_t dy = position_y.access(s,i);   // Load position
            real_t dz = position_z.access(s,i);   // Load position

            // real_t hax  = qdt_2mc*(    ( ex    + dy*dexdy    ) +
            //         dz*( dexdz + dy*d2exdydz ) );
            // real_t hay  = qdt_2mc*(    ( ey    + dz*deydz    ) +
            //         dx*( deydx + dz*d2eydzdx ) );
            // real_t haz  = qdt_2mc*(    ( ez    + dx*dezdx    ) +
            //         dy*( dezdy + dx*d2ezdxdy ) );

            //1D only
            real_t hax = qdt_2mc*ex;
            real_t hay = 0;
            real_t haz = 0;

            cbx  = cbx + dx*dcbxdx;             // Interpolate B
            cby  = cby + dy*dcbydy;
            cbz  = cbz + dz*dcbzdz;

            real_t ux = velocity_x.access(s,i);   // Load velocity
            real_t uy = velocity_y.access(s,i);   // Load velocity
            real_t uz = velocity_z.access(s,i);   // Load velocity

            ux  += hax;                               // Half advance E
            uy  += hay;
            uz  += haz;

            real_t v0   = qdt_2mc/sqrtf(one + (ux*ux + (uy*uy + uz*uz)));
            /**/                                      // Boris - scalars
            real_t v1   = cbx*cbx + (cby*cby + cbz*cbz);
            real_t v2   = (v0*v0)*v1;
            real_t v3   = v0*(one+v2*(one_third+v2*two_fifteenths));
            real_t v4   = v3/(one+v1*(v3*v3));
            v4  += v4;
            v0   = ux + v3*( uy*cbz - uz*cby );       // Boris - uprime
            v1   = uy + v3*( uz*cbx - ux*cbz );
            v2   = uz + v3*( ux*cby - uy*cbx );
            ux  += v4*( v1*cbz - v2*cby );            // Boris - rotation
            uy  += v4*( v2*cbx - v0*cbz );
            uz  += v4*( v0*cby - v1*cbx );
            ux  += hax;                               // Half advance E
            uy  += hay;
            uz  += haz;

            velocity_x.access(s,i) = ux;
            velocity_y.access(s,i) = uy;
            velocity_z.access(s,i) = uz;

            v0   = one/sqrtf(one + (ux*ux+ (uy*uy + uz*uz)));
            /**/                                      // Get norm displacement
            ux  *= cdt_dx;
            uy  *= cdt_dy;
            uz  *= cdt_dz;
            ux  *= v0;
            uy  *= v0;
            uz  *= v0;
            v0   = dx + ux;                           // Streak midpoint (inbnds)
            v1   = dy + uy;
            v2   = dz + uz;
            v3   = v0 + ux;                           // New position
            v4   = v1 + uy;
            real_t v5   = v2 + uz;

            real_t q = weight.access(s,i)*qsp;   // Load charge

            // Check if inbnds
            if(  v3<=one &&  v4<=one &&  v5<=one && -v3<=one && -v4<=one && -v5<=one )
            {

                // Common case (inbnds).  Note: accumulator values are 4 times
                // the total physical charge that passed through the appropriate
                // current quadrant in a time-step


                // Store new position
                position_x.access(s,i) = v3;
                position_y.access(s,i) = v4;
                position_z.access(s,i) = v5;

                dx = v0;                                // Streak midpoint
                dy = v1;
                dz = v2;
                v5 = q*ux*uy*uz*one_third;              // Compute correction

                //real_t* a  = (real_t *)( a0[ii].a );              // Get accumulator

                //1D only
                _a(ii,0) += q*ux;
                _a(ii,1) = 0;
                _a(ii,2) = 0;
                _a(ii,3) = 0;

                // #     define ACCUMULATE_J(X,Y,Z,offset)                                 \
                //                     v4  = q*u##X;   /* v2 = q ux                            */        \
                //                     v1  = v4*d##Y;  /* v1 = q ux dy                         */        \
                //                     v0  = v4-v1;    /* v0 = q ux (1-dy)                     */        \
                //                     v1 += v4;       /* v1 = q ux (1+dy)                     */        \
                //                     v4  = one+d##Z; /* v4 = 1+dz                            */        \
                //                     v2  = v0*v4;    /* v2 = q ux (1-dy)(1+dz)               */        \
                //                     v3  = v1*v4;    /* v3 = q ux (1+dy)(1+dz)               */        \
                //                     v4  = one-d##Z; /* v4 = 1-dz                            */        \
                //                     v0 *= v4;       /* v0 = q ux (1-dy)(1-dz)               */        \
                //                     v1 *= v4;       /* v1 = q ux (1+dy)(1-dz)               */        \
                //                     v0 += v5;       /* v0 = q ux [ (1-dy)(1-dz) + uy*uz/3 ] */        \
                //                     v1 -= v5;       /* v1 = q ux [ (1+dy)(1-dz) - uy*uz/3 ] */        \
                //                     v2 -= v5;       /* v2 = q ux [ (1-dy)(1+dz) - uy*uz/3 ] */        \
                //                     v3 += v5;       /* v3 = q ux [ (1+dy)(1+dz) + uy*uz/3 ] */        \
                //                     _a(ii,offset+0) += v0; \
                //                     _a(ii,offset+1) += v1; \
                //                     _a(ii,offset+2) += v2; \
                //                     _a(ii,offset+3) += v3;

                //                     ACCUMULATE_J( x,y,z, 0 );
                //                     ACCUMULATE_J( y,z,x, 4 );
                //                     ACCUMULATE_J( z,x,y, 8 );

                // #     undef ACCUMULATE_J

            }
            else
            {                                    // Unlikely
                local_pm.dispx = ux;
                local_pm.dispy = uy;
                local_pm.dispz = uz;

                local_pm.i = s*particle_list_t::vector_length + i; //i + itmp; //p_ - p0;

                // Handle particles that cross cells
                move_p( position_x, position_y, position_z, cell, _a, q, local_pm,  g,  s, i, nx, ny, nz, num_ghosts, boundary );

                // TODO: renable this
                //if ( move_p( p0, local_pm, a0, g, qsp ) ) { // Unlikely
                //if ( move_p( particles, local_pm, a0, g, qsp, s, i ) ) { // Unlikely
                //if( nm<max_nm ) {
                //pm[nm++] = local_pm[0];
                //}
                //else {
                //ignore++;                 // Unlikely
                //} // if
                //} // if

                /* // Copied from VPIC Kokkos for reference
                   if( move_p_kokkos( k_particles, k_local_particle_movers,
                   k_accumulators_sa, g, qsp ) ) { // Unlikely
                   if( k_nm(0)<max_nm ) {
                   nm = int(Kokkos::atomic_fetch_add( &k_nm(0), 1 ));
                   if (nm >= max_nm) Kokkos::abort("overran max_nm");
                   copy_local_to_pm(nm);
                   }
                   }
                   */
            }

            //} // end VLEN loop
        };

        Cabana::SimdPolicy<particle_list_t::vector_length,ExecutionSpace>
            vec_policy( 0, particles.size() );
        Cabana::simd_parallel_for( vec_policy, _push, "push()" );
        }

#endif // pic_push_h
