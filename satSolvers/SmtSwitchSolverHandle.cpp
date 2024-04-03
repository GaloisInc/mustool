#include "SmtSwitchSolverHandle.h"

using namespace smt;

SmtSwitchSolverHandle::SmtSwitchSolverHandle(SmtSolver & s, std::vector<Term> t): SatSolver(""){
    solver = s;
    Sort boolSort = s->make_sort(SortKind::BOOL);
    for (auto &a: t) {
        std::string control_name = "ceasar" + std::to_string(clauses.size()); //name and create control variables
        Term control = s->make_symbol(control_name.c_str(), boolSort);
        Term impl = s->make_term(Implies, control, a);
        controls.push_back(control);
        implications.push_back(impl);
        clauses.push_back(a);
    }
    dimension = clauses.size();
    for (auto & impl: implications) s->assert_formula(impl);

}

bool SmtSwitchSolverHandle::solve(std::vector<bool> &formula, bool core, bool grow) {
    checks++;
    bool result;
    std::vector<Term> assumptions;
    for(int i = 0; i < formula.size(); i++)
        if(formula[i])
            assumptions.push_back(controls[i]);
        else
            assumptions.push_back(solver->make_term(Not, controls[i]));

    Result r = solver->check_sat_assuming(assumptions);
    if (r == smt::UNSAT) {
        result = false;
    } else if (r == smt::SAT) {
        result = true;
    } else {
        throw std::runtime_error("Solver returned unknown");
    }

    return result;
}

std::string SmtSwitchSolverHandle::toString(std::vector<bool> &mus) {
    throw std::runtime_error("Not implemented"); // TODO(rperoutka)
}

