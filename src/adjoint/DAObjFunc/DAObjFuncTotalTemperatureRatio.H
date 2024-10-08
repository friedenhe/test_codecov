/*---------------------------------------------------------------------------*\

    DAFoam  : Discrete Adjoint with OpenFOAM
    Version : v4

    Description:
        Child class for total temperature ratio

\*---------------------------------------------------------------------------*/

#ifndef DAObjFuncTotalTemperatureRatio_H
#define DAObjFuncTotalTemperatureRatio_H

#include "DAObjFunc.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
      Class DAObjFuncTotalTemperatureRatio Declaration
\*---------------------------------------------------------------------------*/

class DAObjFuncTotalTemperatureRatio
    : public DAObjFunc
{

protected:

    /// the area of all inlet patches
    scalar areaSumInlet_ = -9999.0;

    /// the area of all outlet patches
    scalar areaSumOutlet_ = -9999.0;

    /// names of outlet patches
    wordList outletPatches_;

    /// names of inlet patches
    wordList inletPatches_;

    scalar TTInRef_ = 0.0;

    scalar TTOutRef_ = 0.0;

    scalar Cp_;

    scalar gamma_;

public:
    TypeName("totalTemperatureRatio");
    // Constructors

    //- Construct from components
    DAObjFuncTotalTemperatureRatio(
        const fvMesh& mesh,
        const DAOption& daOption,
        const DAModel& daModel,
        const DAIndex& daIndex,
        const DAResidual& daResidual,
        const word objFuncName,
        const word objFuncPart,
        const dictionary& objFuncDict);

    //- Destructor
    virtual ~DAObjFuncTotalTemperatureRatio()
    {
    }

    /// calculate the value of objective function
    virtual void calcObjFunc(
        const labelList& objFuncFaceSources,
        const labelList& objFuncCellSources,
        scalarList& objFuncFaceValues,
        scalarList& objFuncCellValues,
        scalar& objFuncValue);
};

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
