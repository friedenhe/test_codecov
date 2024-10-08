/*---------------------------------------------------------------------------*\

    DAFoam  : Discrete Adjoint with OpenFOAM
    Version : v4

\*---------------------------------------------------------------------------*/

#include "DATurboFoam.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

defineTypeNameAndDebug(DATurboFoam, 0);
addToRunTimeSelectionTable(DASolver, DATurboFoam, dictionary);
// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

DATurboFoam::DATurboFoam(
    char* argsAll,
    PyObject* pyOptions)
    : DASolver(argsAll, pyOptions),
      simplePtr_(nullptr),
      pThermoPtr_(nullptr),
      pPtr_(nullptr),
      rhoPtr_(nullptr),
      UPtr_(nullptr),
      URelPtr_(nullptr),
      phiPtr_(nullptr),
      pressureControlPtr_(nullptr),
      turbulencePtr_(nullptr),
      daTurbulenceModelPtr_(nullptr),
      MRFPtr_(nullptr),
      initialMass_(dimensionedScalar("initialMass", dimensionSet(1, 0, 0, 0, 0, 0, 0), 0.0))
{
}
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

void DATurboFoam::initSolver()
{
    /*
    Description:
        Initialize variables for DASolver
    */

    Info << "Initializing fields for DATurboFoam" << endl;
    Time& runTime = runTimePtr_();
    fvMesh& mesh = meshPtr_();
    argList& args = argsPtr_();
#include "createSimpleControlPython.H"
#include "createFieldsTurbo.H"
#include "createAdjointCompressible.H"
    // initialize checkMesh
    daCheckMeshPtr_.reset(new DACheckMesh(daOptionPtr_(), runTime, mesh));

    daLinearEqnPtr_.reset(new DALinearEqn(mesh, daOptionPtr_()));

    this->setDAObjFuncList();
}

label DATurboFoam::solvePrimal(
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

#include "createRefsTurbo.H"

    // change the run status
    daOptionPtr_->setOption<word>("runStatus", "solvePrimal");

    // call correctNut, this is equivalent to turbulence->validate();
    daTurbulenceModelPtr_->updateIntermediateVariables();

    Info << "\nStarting time loop\n"
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

    // if the forwardModeAD is active, we need to set the seed here
#include "setForwardADSeeds.H"

    // if useMeanStates is used, we need to zero meanStates before the primal run
    this->zeroMeanStates();

    label printInterval = daOptionPtr_->getOption<label>("printInterval");
    label printToScreen = 0;
    label regModelFail = 0;
    while (this->loop(runTime)) // using simple.loop() will have seg fault in parallel
    {
        DAUtility::primalMaxInitRes_ = -1e16;

        printToScreen = this->isPrintTime(runTime, printInterval);

        if (printToScreen)
        {
            Info << "Time = " << runTime.timeName() << nl << endl;
        }

        p.storePrevIter();
        rho.storePrevIter();

        // Pressure-velocity SIMPLE corrector
#include "UEqnTurbo.H"
#include "pEqnTurbo.H"
#include "EEqnTurbo.H"

        daTurbulenceModelPtr_->correct(printToScreen);

        // update the output field value at each iteration, if the regression model is active
        regModelFail = daRegressionPtr_->compute();

        if (this->validateStates())
        {
            // write data to files and quit
            runTime.writeNow();
            mesh.write();
            return 1;
        }

        if (printToScreen)
        {
            daTurbulenceModelPtr_->printYPlus();

            this->printAllObjFuncs();

            daRegressionPtr_->printInputInfo();

            Info << "ExecutionTime = " << runTime.elapsedCpuTime() << " s"
                 << "  ClockTime = " << runTime.elapsedClockTime() << " s"
                 << nl << endl;
        }

        // if useMeanStates is used, we need to calculate the meanStates
        this->calcMeanStates();

        runTime.write();
    }

    if (regModelFail != 0)
    {
        return 1;
    }

    // if useMeanStates is used, we need to assign meanStates to states right after the case converges
    this->assignMeanStatesToStates();

    this->calcPrimalResidualStatistics("print");

    // primal converged, assign the OpenFoam fields to the state vec wVec
    this->ofField2StateVec(wVec);

    // write the mesh to files
    mesh.write();

    Info << "End\n"
         << endl;

    return this->checkResidualTol();
}

} // End namespace Foam

// ************************************************************************* //
