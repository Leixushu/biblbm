/* This file is part of the Palabos library.
 *
 * Copyright (C) 2011-2013 FlowKit Sarl
 * Route d'Oron 2
 * 1010 Lausanne, Switzerland
 * E-mail contact: contact@flowkit.com
 *
 * The most recent release of Palabos can be downloaded at 
 * <http://www.palabos.org/>
 *
 * The library Palabos is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * The library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef INTERPOLATIONS_2D_HH
#define INTERPOLATIONS_2D_HH

#include "core/globalDefs.h"
#include "core/util.h"
#include "finiteDifference/interpolations2D.h"
#include <vector>

namespace plb {

/* ******** Function linearInterpolationCoefficients ********************* */

template<typename T>
void linearInterpolationCoefficients (
        AtomicBlock2D const& block, Array<T,2> const& position,
        std::vector<Dot2D>& cellPos, std::vector<T>& weights )
{
    cellPos.resize(4);
    cellPos[0] = Dot2D( (plint) position[0],         (plint) position[1]        );
    cellPos[1] = Dot2D( (plint) position[0],         (plint)(position[1]+(T)1.0));
    cellPos[2] = Dot2D( (plint)(position[0]+(T)1.0), (plint) position[1]        );
    cellPos[3] = Dot2D( (plint)(position[0]+(T)1.0), (plint)(position[1]+(T)1.0));

    T u = position[0] - (T)cellPos[0].x;
    T v = position[1] - (T)cellPos[0].y;

    weights.resize(4);
    weights[0] = (1.-u) * (1.-v);
    weights[1] = (1.-u) * (   v);
    weights[2] = (   u) * (1.-v);
    weights[3] = (   u) * (   v);

    // Convert cell position to local coordinates.
    for (plint iPos=0; iPos<4; ++iPos) {
        cellPos[iPos] -= block.getLocation();
    }
}


template<typename T, plint nDim>
Array<T,nDim> linearInterpolateTensorField (
        TensorField2D<T,nDim>& tensorField, Array<T,2> const& position )
{
    std::vector<Dot2D> pos(4);
    std::vector<T> weights(4);
    linearInterpolationCoefficients(tensorField, position, pos, weights);
    Array<T,2> vector;
    vector.resetToZero();
    for (plint iCell=0; iCell<4; ++iCell) {
        vector += weights[iCell]*tensorField.get(pos[iCell].x,pos[iCell].y);
    }

    return vector;
}

template<typename T, plint nDim>
Array<T,nDim> predictorCorrectorTensorField (
        TensorField2D<T,nDim>& tensorField, Array<T,2> const& position, T scaling )
{
    Array<T,2> position1(position);
    std::vector<Dot2D> pos(4);
    std::vector<T> weights(4);
    linearInterpolationCoefficients(tensorField, position1, pos, weights);
    Array<T,2> vector1;
    vector1.resetToZero();
    for (plint iCell=0; iCell<4; ++iCell) {
        vector1 += weights[iCell]*tensorField.get(pos[iCell].x,pos[iCell].y)*scaling;
    }

    Array<T,2> position2(position1+vector1);
    linearInterpolationCoefficients(tensorField, position2, pos, weights);
    Array<T,2> vector2;
    vector2.resetToZero();
    for (plint iCell=0; iCell<4; ++iCell) {
        vector2 += weights[iCell]*tensorField.get(pos[iCell].x,pos[iCell].y)*scaling;
    }

    return (vector1+vector2)/(T)2;
}

//// ADDED BY ANDREA

template<typename T>
void predictorCorrectorRhoBarJ (
        NTensorField2D<T>& rhoBarJ, Array<T,2> const& position,
        bool velIsJ, Array<T,2>& j, T& rhoBar )
{
    PLB_ASSERT( rhoBarJ.getNdim()==3 );
    Array<T,2> position1(position);
    std::vector<Dot2D> pos(4);
    std::vector<T> weights(4);
    linearInterpolationCoefficients(rhoBarJ, position1, pos, weights);
    Array<T,2> j1;
    j1.resetToZero();
    T rhoBar1 = T();
    for (plint iCell=0; iCell<4; ++iCell) {
        T const* data = rhoBarJ.get(pos[iCell].x,pos[iCell].y);
        j1.add_from_cArray(data+1, weights[iCell]);
        rhoBar1 += weights[iCell]*(*data);
    }

    Array<T,2> position2(position1+j1);
    linearInterpolationCoefficients(rhoBarJ, position2, pos, weights);
    Array<T,2> j2;
    j2.resetToZero();
    T rhoBar2 = T();
    for (plint iCell=0; iCell<4; ++iCell) {
        T const* data = rhoBarJ.get(pos[iCell].x,pos[iCell].y);
        j2.add_from_cArray(data+1, weights[iCell]);
        rhoBar2 += weights[iCell]*(*data);
    }

    j= (j1+j2)/(T)2;
    rhoBar = (rhoBar1+rhoBar2)/(T)2;
}


template<typename T>
T linearInterpolateScalarField (
        ScalarField2D<T>& scalarField, Array<T,2> const& position )
{
    std::vector<Dot2D> pos(4);
    std::vector<T> weights(4);
    linearInterpolationCoefficients(scalarField, position, pos, weights);
    T scalar = T();
    for (plint iCell=0; iCell<4; ++iCell) {
        scalar += weights[iCell]*scalarField.get(pos[iCell].x,pos[iCell].y);
    }

    return scalar;
}

}  // namespace plb

#endif  // INTERPOLATIONS_2D_HH

