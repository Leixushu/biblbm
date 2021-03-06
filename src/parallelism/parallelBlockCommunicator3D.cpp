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
 * Helper classes for parallel 3D multiblock lattice -- generic implementation.
 */
#ifndef PARALLEL_BLOCK_COMMUNICATOR_3D_HH
#define PARALLEL_BLOCK_COMMUNICATOR_3D_HH

#include "parallelism/parallelBlockCommunicator3D.h"
#include "multiBlock/multiBlock3D.h"
#include "multiBlock/multiBlockManagement3D.h"
#include "atomicBlock/atomicBlock3D.h"
#include "core/plbDebug.h"
#include "core/plbProfiler.h"
#include <algorithm>

namespace plb {

#ifdef PLB_MPI_PARALLEL

CommunicationStructure3D::CommunicationStructure3D (
        std::vector<Overlap3D> const& overlaps,
        MultiBlockManagement3D const& originManagement,
        MultiBlockManagement3D const& destinationManagement,
        plint sizeOfCell )
{
    plint fromEnvelopeWidth = originManagement.getEnvelopeWidth();
    plint toEnvelopeWidth = destinationManagement.getEnvelopeWidth();
    SparseBlockStructure3D const& fromSparseBlock
        = originManagement.getSparseBlockStructure();
    SparseBlockStructure3D const& toSparseBlock
        = destinationManagement.getSparseBlockStructure();

    SendRecvPool sendPool, recvPool;
    for (pluint iOverlap=0; iOverlap<overlaps.size(); ++iOverlap) {
        Overlap3D const& overlap = overlaps[iOverlap];
        CommunicationInfo3D info;

        info.fromBlockId = overlap.getOriginalId();
        info.toBlockId   = overlap.getOverlapId();

        SmartBulk3D originalBulk(fromSparseBlock, fromEnvelopeWidth, info.fromBlockId);
        SmartBulk3D overlapBulk(toSparseBlock, toEnvelopeWidth, info.toBlockId);

        Box3D originalCoordinates(overlap.getOriginalCoordinates());
        Box3D overlapCoordinates(overlap.getOverlapCoordinates());
        info.fromDomain = originalBulk.toLocal(originalCoordinates);
        info.toDomain   = overlapBulk.toLocal(overlapCoordinates);
        info.absoluteOffset = Dot3D (
                overlapCoordinates.x0 - originalCoordinates.x0,
                overlapCoordinates.y0 - originalCoordinates.y0,
                overlapCoordinates.z0 - originalCoordinates.z0 );

        plint lx = info.fromDomain.x1-info.fromDomain.x0+1;
        plint ly = info.fromDomain.y1-info.fromDomain.y0+1;
        plint lz = info.fromDomain.z1-info.fromDomain.z0+1;
        PLB_PRECONDITION(lx == info.toDomain.x1-info.toDomain.x0+1);
        PLB_PRECONDITION(ly == info.toDomain.y1-info.toDomain.y0+1);
        PLB_PRECONDITION(lz == info.toDomain.z1-info.toDomain.z0+1);

        plint numberOfCells = lx*ly*lz;

        ThreadAttribution const& fromAttribution = originManagement.getThreadAttribution();
        ThreadAttribution const& toAttribution = destinationManagement.getThreadAttribution();
        info.fromProcessId = fromAttribution.getMpiProcess(info.fromBlockId);
        info.toProcessId   = toAttribution.getMpiProcess(info.toBlockId);

        if ( fromAttribution.isLocal(info.fromBlockId) &&
             toAttribution.isLocal(info.toBlockId))
        {
            sendRecvPackage.push_back(info);
        }
        else if (fromAttribution.isLocal(info.fromBlockId))
        {
            sendPackage.push_back(info);
            sendPool.subscribeMessage(info.toProcessId, numberOfCells*sizeOfCell);
        }
        else if (toAttribution.isLocal(info.toBlockId))
        {
            recvPackage.push_back(info);
            recvPool.subscribeMessage(info.fromProcessId, numberOfCells*sizeOfCell);
        }
    }

    sendComm = SendPoolCommunicator(sendPool);
    recvComm = RecvPoolCommunicator(recvPool);
}

////////////////////// Class ParallelBlockCommunicator3D /////////////////////

ParallelBlockCommunicator3D::ParallelBlockCommunicator3D()
    : overlapsModified(true),
      communication(0)
{ }

ParallelBlockCommunicator3D::ParallelBlockCommunicator3D (
        ParallelBlockCommunicator3D const& rhs )
    : overlapsModified(true),
      communication(0)
{ }

ParallelBlockCommunicator3D::~ParallelBlockCommunicator3D() {
    delete communication;
}

ParallelBlockCommunicator3D& ParallelBlockCommunicator3D::operator= (
        ParallelBlockCommunicator3D const& rhs )
{
    ParallelBlockCommunicator3D(rhs).swap(*this);
    return *this;
}

void ParallelBlockCommunicator3D::swap(ParallelBlockCommunicator3D& rhs) {
    std::swap(overlapsModified,rhs.overlapsModified);
    std::swap(communication,rhs.communication);
}

ParallelBlockCommunicator3D* ParallelBlockCommunicator3D::clone() const {
    return new ParallelBlockCommunicator3D(*this);
}

void ParallelBlockCommunicator3D::duplicateOverlaps( MultiBlock3D& multiBlock,
                                                     modif::ModifT whichData ) const
{
    MultiBlockManagement3D const& multiBlockManagement = multiBlock.getMultiBlockManagement();
    PeriodicitySwitch3D const& periodicity             = multiBlock.periodicity();

    // Implement a caching mechanism for the communication structure.
    if (overlapsModified) {
        overlapsModified = false;
        LocalMultiBlockInfo3D const& localInfo = multiBlockManagement.getLocalInfo();
        std::vector<Overlap3D> overlaps(multiBlockManagement.getLocalInfo().getNormalOverlaps());
        for (pluint iOverlap=0; iOverlap<localInfo.getPeriodicOverlaps().size(); ++iOverlap) {
            PeriodicOverlap3D const& pOverlap = localInfo.getPeriodicOverlaps()[iOverlap];
            if (periodicity.get(pOverlap.normalX,pOverlap.normalY,pOverlap.normalZ)) {
                overlaps.push_back(pOverlap.overlap);
            }
        }
        delete communication;
        communication = new CommunicationStructure3D (
                                overlaps,
                                multiBlockManagement, multiBlockManagement,
                                multiBlock.sizeOfCell() );
    }

    communicate(*communication, multiBlock, multiBlock, whichData);
}

void ParallelBlockCommunicator3D::communicate (
        std::vector<Overlap3D> const& overlaps,
        MultiBlock3D const& originMultiBlock,
        MultiBlock3D& destinationMultiBlock, modif::ModifT whichData ) const
{
    PLB_PRECONDITION( originMultiBlock.sizeOfCell() ==
                      destinationMultiBlock.sizeOfCell() );

    CommunicationStructure3D communication (
            overlaps,
            originMultiBlock.getMultiBlockManagement(),
            destinationMultiBlock.getMultiBlockManagement(),
            originMultiBlock.sizeOfCell() );
    communicate(communication, originMultiBlock, destinationMultiBlock, whichData);
}

void ParallelBlockCommunicator3D::communicate (
        CommunicationStructure3D& communication,
        MultiBlock3D const& originMultiBlock,
        MultiBlock3D& destinationMultiBlock, modif::ModifT whichData ) const
{
    global::profiler().start("mpiCommunication");
    bool staticMessage = whichData == modif::staticVariables;
    // 1. Non-blocking receives.
    communication.recvComm.startBeingReceptive(staticMessage);

    // 2. Non-blocking sends.
    for (unsigned iSend=0; iSend<communication.sendPackage.size(); ++iSend) {
        CommunicationInfo3D const& info = communication.sendPackage[iSend];
        AtomicBlock3D const& fromBlock = originMultiBlock.getComponent(info.fromBlockId);
        fromBlock.getDataTransfer().send (
                info.fromDomain, communication.sendComm.getSendBuffer(info.toProcessId),
                whichData );
        communication.sendComm.acceptMessage(info.toProcessId, staticMessage);
    }

    // 3. Local copies which require no communication.
    for (unsigned iSendRecv=0; iSendRecv<communication.sendRecvPackage.size(); ++iSendRecv) {
        CommunicationInfo3D const& info = communication.sendRecvPackage[iSendRecv];
        AtomicBlock3D const& fromBlock = originMultiBlock.getComponent(info.fromBlockId);
        AtomicBlock3D& toBlock = destinationMultiBlock.getComponent(info.toBlockId);
        plint deltaX = info.fromDomain.x0 - info.toDomain.x0;
        plint deltaY = info.fromDomain.y0 - info.toDomain.y0;
        plint deltaZ = info.fromDomain.z0 - info.toDomain.z0;
        toBlock.getDataTransfer().attribute (
                info.toDomain, deltaX, deltaY, deltaZ, fromBlock,
                whichData, info.absoluteOffset );
    }

    // 4. Finalize the receives.
    for (unsigned iRecv=0; iRecv<communication.recvPackage.size(); ++iRecv) {
        CommunicationInfo3D const& info = communication.recvPackage[iRecv];
        AtomicBlock3D& toBlock = destinationMultiBlock.getComponent(info.toBlockId);
        toBlock.getDataTransfer().receive (
                info.toDomain,
                communication.recvComm.receiveMessage(info.fromProcessId, staticMessage),
                whichData, info.absoluteOffset );
    }

    // 5. Finalize the sends.
    communication.sendComm.finalize(staticMessage);
    global::profiler().stop("mpiCommunication");
}

void ParallelBlockCommunicator3D::signalPeriodicity() const {
    overlapsModified = true;
}

#endif  // PLB_MPI_PARALLEL

}  // namespace plb

#endif  // PARALLEL_BLOCK_COMMUNICATOR_3D_HH
