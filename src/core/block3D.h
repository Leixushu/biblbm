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


#ifndef BLOCK_3D_H
#define BLOCK_3D_H

#include "core/globalDefs.h"
#include "core/blockIdentifiers.h"
#include "core/serializer.h"
#include "core/blockStatistics.h"
#include "core/geometry3D.h"

namespace plb {

class Block3D {
public:
    virtual ~Block3D() { }
    virtual Box3D getBoundingBox() const =0;
    virtual DataSerializer* getBlockSerializer (
            Box3D const& domain, IndexOrdering::OrderingT ordering ) const =0;
    virtual DataUnSerializer* getBlockUnSerializer (
            Box3D const& domain, IndexOrdering::OrderingT ordering ) =0;
};

void copySerializedBlock( Block3D const& from, Block3D& to,
                          IndexOrdering::OrderingT ordering = IndexOrdering::forward );

/// Some end-user implementations of the Block3D have a static cache-policy class,
///   which can be access to fine-tune the performance on a given platform.
class CachePolicy3D {
public:
    CachePolicy3D(plint blockSize_) : blockSize(blockSize_)
    { }
    void setBlockSize(plint blockSize_) {
        blockSize = blockSize_;
    }
    plint getBlockSize() const {
        return blockSize;
    }
private:
    plint blockSize;
};

} // namespace plb

#endif  // BLOCK_3D_H
