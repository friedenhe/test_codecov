/*---------------------------------------------------------------------------*\

    DAFoam  : Discrete Adjoint with OpenFOAM
    Version : v4

    This class is modified from OpenFOAM's source code
    applications/solvers/basic/scalarTransportFoam

    OpenFOAM: The Open Source CFD Toolbox

    Copyright (C): 2011-2016 OpenFOAM Foundation

    OpenFOAM License:

        OpenFOAM is free software: you can redistribute it and/or modify it
        under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
        (at your option) any later version.
    
        OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
        ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
        FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
        for more details.
    
        You should have received a copy of the GNU General Public License
        along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "DAScalarTransportFoam.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

defineTypeNameAndDebug(DAScalarTransportFoam, 0);
addToRunTimeSelectionTable(DASolver, DAScalarTransportFoam, dictionary);
// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

DAScalarTransportFoam::DAScalarTransportFoam(
    char* argsAll,
    PyObject* pyOptions)
    : DASolver(argsAll, pyOptions),
      TPtr_(nullptr),
      UPtr_(nullptr),
      phiPtr_(nullptr),
      DTPtr_(nullptr)
{
}
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

void DAScalarTransportFoam::initSolver()
{
    /*
    Description:
        Initialize variables for DASolver
    */

    Info << "Initializing fields for DAScalarTransportFoam" << endl;
    Time& runTime = runTimePtr_();
    fvMesh& mesh = meshPtr_();
#include "createFieldsScalarTransport.H"
#include "createAdjointSolid.H"
    // initialize checkMesh
    daCheckMeshPtr_.reset(new DACheckMesh(daOptionPtr_(), runTime, mesh));

    daLinearEqnPtr_.reset(new DALinearEqn(mesh, daOptionPtr_()));

    this->setDAObjFuncList();

    mode_ = daOptionPtr_->getSubDictOption<word>("unsteadyAdjoint", "mode");

    if (mode_ == "hybrid")
    {

        nTimeInstances_ =
            daOptionPtr_->getSubDictOption<label>("unsteadyAdjoint", "nTimeInstances");

        periodicity_ =
            daOptionPtr_->getSubDictOption<scalar>("unsteadyAdjoint", "periodicity");

        if (periodicity_ <= 0)
        {
            FatalErrorIn("") << "periodicity <= 0!" << abort(FatalError);
        }

        if (nTimeInstances_ <= 0)
        {
            FatalErrorIn("") << "nTimeInstances <= 0!" << abort(FatalError);
        }

        stateAllInstances_.setSize(nTimeInstances_);
        stateBoundaryAllInstances_.setSize(nTimeInstances_);
        objFuncsAllInstances_.setSize(nTimeInstances_);
        runTimeAllInstances_.setSize(nTimeInstances_);
        runTimeIndexAllInstances_.setSize(nTimeInstances_);

        forAll(stateAllInstances_, idxI)
        {
            stateAllInstances_[idxI].setSize(daIndexPtr_->nLocalAdjointStates);
            stateBoundaryAllInstances_[idxI].setSize(daIndexPtr_->nLocalAdjointBoundaryStates);
            runTimeAllInstances_[idxI] = 0.0;
            runTimeIndexAllInstances_[idxI] = 0;
        }
    }
}

label DAScalarTransportFoam::solvePrimal(
    const Vec xvVec,
    Vec wVec)
{
    /*
    Description:
        Call the primal solver to get converged state variables

    Input:
        xvVec: a vector that contains all volume mesh coordinates

    Output:
        wVec: state variable vector
    */

#include "createRefsScalarTransport.H"

    // change the run status
    daOptionPtr_->setOption<word>("runStatus", "solvePrimal");

    Info << "\nCalculating temperature distribution\n"
         << endl;

    // deform the mesh based on the xvVec
    this->pointVec2OFMesh(xvVec);

    // check mesh quality
    label meshOK = this->checkMesh();

    if (!meshOK)
    {
        this->writeFailedMesh();
        return 1;
    }

    // We need to set the mesh moving to false, otherwise we will get V0 not found error.
    // Need to dig into this issue later
    // NOTE: we have commented this out. Setting mesh.moving(false) has been done
    // right after mesh.movePoints() calls.
    //mesh.moving(false);

    label printInterval = daOptionPtr_->getOption<label>("printIntervalUnsteady");
    label printToScreen = 0;
    label timeInstanceI = 0;

    // main loop
    while (this->loop(runTime)) // using simple.loop() will have seg fault in parallel
    {
        DAUtility::primalMaxInitRes_ = -1e16;

        printToScreen = this->isPrintTime(runTime, printInterval);

        if (printToScreen)
        {
            Info << "Time = " << runTime.timeName() << nl << endl;
        }

        fvScalarMatrix TEqn(
            fvm::ddt(T)
            + fvm::div(phi, T)
            - fvm::laplacian(DT, T));

        TEqn.relax();

        // get the solver performance info such as initial
        // and final residuals
        SolverPerformance<scalar> solverT = TEqn.solve();
        DAUtility::primalResidualControl(solverT, printToScreen, "T");

        if (this->validateStates())
        {
            // write data to files and quit
            runTime.writeNow();
            mesh.write();
            return 1;
        }

        if (printToScreen)
        {

            this->printAllObjFuncs();

            if (daOptionPtr_->getOption<label>("debug"))
            {
                this->calcPrimalResidualStatistics("print");
            }

            Info << "ExecutionTime = " << runTime.elapsedCpuTime() << " s"
                 << "  ClockTime = " << runTime.elapsedClockTime() << " s"
                 << nl << endl;
        }

        runTime.write();

        if (mode_ == "hybrid")
        {
            this->saveTimeInstanceFieldHybrid(timeInstanceI);
        }
    }

    this->calcPrimalResidualStatistics("print");

    // primal converged, assign the OpenFoam fields to the state vec wVec
    this->ofField2StateVec(wVec);

    // write the mesh to files
    mesh.write();

    Info << "End\n"
         << endl;

    return 0;
}

} // End namespace Foam

// ************************************************************************* //
