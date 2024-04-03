#include <string>
#include <vector>
#include "SatSolver.h"
#include "smt-switch/smt.h"
#include "smt-switch/smt_defs.h"

class SmtSwitchSolverHandle: public SatSolver{
public:

    smt::SmtSolver solver;
    std::vector<smt::Term> controls;
    std::vector<smt::Term> implications;
    std::vector<smt::Term> clauses;

    SmtSwitchSolverHandle(smt::SmtSolver & s, std::vector<smt::Term> t);
    ~SmtSwitchSolverHandle() = default;
    bool solve(std::vector<bool> &formula, bool core = false, bool grow = false);
    std::string toString(std::vector<bool> &mus);
};
