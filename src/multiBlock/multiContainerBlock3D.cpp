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

/** \file
 * Serial implementation of scalar, vector and tensor fields for 3D data analysis.
 * -- header file
 */

#include "core/globalDefs.h"
#include "multiBlock/multiContainerBlock3D.h"
#include "multiBlock/defaultMultiBlockPolicy3D.h"
#include "core/blockIdentifiers.h"

namespace plb {

MultiContainerBlock3D::MultiContainerBlock3D (
        MultiBlockManagement3D const& multiBlockManagement_,
        CombinedStatistics* combinedStatistics_ )

    : MultiBlock3D(multiBlockManagement_,
                   defaultMultiBlockPolicy3D().getBlockCommunicator(),
                   combinedStatistics_)
{
    allocateBlocks();
}

MultiContainerBlock3D::~MultiContainerBlock3D() {
    deAllocateBlocks();
}

MultiContainerBlock3D::MultiContainerBlock3D(plint nx_, plint ny_, plint nz_)
    : MultiBlock3D (
            // Default envelope-width to 0
            defaultMultiBlockPolicy3D().getMultiBlockManagement(nx_,ny_,nz_, 0),
            defaultMultiBlockPolicy3D().getBlockCommunicator(),
            defaultMultiBlockPolicy3D().getCombinedStatistics() )
{
    allocateBlocks();
}

MultiContainerBlock3D::MultiContainerBlock3D(MultiBlock3D const& rhs)
    : MultiBlock3D(rhs)
{
    allocateBlocks();
}

MultiContainerBlock3D::MultiContainerBlock3D(MultiBlock3D const& rhs, Box3D subDomain, bool crop)
    : MultiBlock3D (
            intersect(rhs.getMultiBlockManagement(), subDomain, crop),
            rhs.getBlockCommunicator().clone(),
            rhs.getCombinedStatistics().clone() )
{
    allocateBlocks();
}

MultiContainerBlock3D::MultiContainerBlock3D(MultiContainerBlock3D const& rhs)
    : MultiBlock3D(rhs)
{
    allocateBlocks();
}

MultiContainerBlock3D& MultiContainerBlock3D::operator= (
        MultiContainerBlock3D const& rhs )
{
    MultiContainerBlock3D tmp(rhs);
    swap(tmp);
    return *this;
}

MultiContainerBlock3D* MultiContainerBlock3D::clone() const {
    return new MultiContainerBlock3D(*this);
}

MultiContainerBlock3D* MultiContainerBlock3D::clone (
        MultiBlockManagement3D const& multiBlockManagement ) const
{
    // By definition, a multi container block cannot be redistributed over
    //   a different block arrangement.
    PLB_ASSERT( false );
    return 0;
}

void MultiContainerBlock3D::swap(MultiContainerBlock3D& rhs) {
    blocks.swap(rhs.blocks);
    MultiBlock3D::swap(rhs);
}

void MultiContainerBlock3D::allocateBlocks() 
{
    for (pluint iBlock=0; iBlock<this->getLocalInfo().getBlocks().size(); ++iBlock)
    {
        plint blockId = this->getLocalInfo().getBlocks()[iBlock];
        SmartBulk3D bulk(this->getMultiBlockManagement(), blockId);
        Box3D envelope = bulk.computeEnvelope();
        AtomicContainerBlock3D* newBlock =
            new AtomicContainerBlock3D (
                    envelope.getNx(), envelope.getNy(), envelope.getNz() );
        newBlock -> setLocation(Dot3D(envelope.x0, envelope.y0, envelope.z0));
        blocks[blockId] = newBlock;
    }
}

void MultiContainerBlock3D::deAllocateBlocks() 
{
    for (BlockMap::iterator it = blocks.begin(); it != blocks.end(); ++it)
    {
        delete it->second;
    }
}

AtomicContainerBlock3D& MultiContainerBlock3D::getComponent(plint blockId)
{
    BlockMap::iterator it = blocks.find(blockId);
    PLB_ASSERT (it != blocks.end());
    return *it->second;
}

AtomicContainerBlock3D const& MultiContainerBlock3D::
    getComponent(plint blockId) const
{
    BlockMap::const_iterator it = blocks.find(blockId);
    PLB_ASSERT (it != blocks.end());
    return *it->second;
}

plint MultiContainerBlock3D::sizeOfCell() const {
    return 0;
}

plint MultiContainerBlock3D::getCellDim() const {
    return 0;
}

int MultiContainerBlock3D::getStaticId() const {
    return 0;
}

void MultiContainerBlock3D::copyReceive (
                MultiBlock3D const& fromBlock, Box3D const& fromDomain,
                Box3D const& toDomain, modif::ModifT whichData )
{
    PLB_ASSERT( false );
}

std::string MultiContainerBlock3D::getBlockName() const {
    return std::string("ContainerBlock3D");
}

std::vector<std::string> MultiContainerBlock3D::getTypeInfo() const {
    return std::vector<std::string>();
}


MultiContainerBlock3D* createContainerBlock(MultiBlock3D& templ, ContainerBlockData* data)
{
    MultiContainerBlock3D* dataContainer = new MultiContainerBlock3D(templ);

    MultiBlockManagement3D const& management = templ.getMultiBlockManagement();
    ThreadAttribution const& threadAttribution = management.getThreadAttribution();
    SparseBlockStructure3D const& sparseBlock = management.getSparseBlockStructure();
    std::map<plint,Box3D> const& domains = sparseBlock.getBulks();

    std::map<plint,Box3D>::const_iterator it = domains.begin();
    plint pos = 0;
    for (; it != domains.end(); ++it, ++pos) {
        plint id = it->first;
        if (threadAttribution.isLocal(id)) {
            AtomicContainerBlock3D& atomicDataContainer = dataContainer->getComponent(id);
            ContainerBlockData* nextData = data->clone();
            nextData->setUniqueID(pos);
            atomicDataContainer.setData(nextData);
        }
    }

    delete data;
    return dataContainer;
}

}  // namespace plb
