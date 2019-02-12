#include "Master.h"
#include "misc.h"
#include <algorithm>
#include <math.h>
#include <functional>
#include <random>


Master::Master(string filename, int var, bool vis, string s_solver){
        isValidExecutions = 0;
        variant = var;
	sat_solver = s_solver;
	if(ends_with(filename, "smt2")){
		satSolver = new Z3Handle(filename); 
		domain = "smt";
	}
	else if(ends_with(filename, "cnf")){
		satSolver = new MSHandle(filename);
		domain = "sat";
	}
	else if(ends_with(filename, "ltl")){
		if(sat_solver == "nuxmv")
			satSolver = new NuxmvHandle(filename); 
		else
			satSolver = new SpotHandle(filename);
		domain = "ltl";
	}
	else
		print_err("wrong input file");
	dimension = satSolver->dimension;	
	cout << "Dimension:" << dimension << endl;
        explorer = new Explorer(dimension);	
	explorer->satSolver = satSolver;
        verbose = false;
	depthMUS = 0;
	extend_top_variant = 0;
	dim_reduction = 0.5;
	output_file = "";
	validate_mus_c = false;
	hsd = false;
	hsd_succ = 0;
	verify_approx = false;
	total_difference_of_approximated_muses = 0;
	overapproximated_muses = rightly_approximated_muses = 0;
	current_depth = 0;
	explored_extensions = 0;
	spurious_muses = 0;
	skipped_shrinks = 0;
	unex_sat = unex_unsat = known_unex_unsat = extracted_unex = 0;
	rotated_muses = 0;
	duplicated = 0;
	max_round = 0;
	explicit_seeds = 0;

	scope_limit = 100;	

	use_edge_muses = true;
	rot_muses.resize(dimension, unordered_map<int, vector<int>>());

	backbone_crit_value.resize(dimension, false);
	backbone_crit_used.resize(dimension, false);

        hash = random_number();
	satSolver->hash = hash;
	used_backbones_count = 0;
	original_backbones_count = 0;
}

Master::~Master(){
	delete satSolver;
	delete explorer;
}

void Master::write_mus_to_file(MUS& f){
	if(satSolver->clauses_string.size() != dimension)
		print_err("write_mus_to_file error");
	ofstream outfile;
	outfile.open(output_file, std::ios_base::app);
	for(auto c: f.int_mus)
		outfile << satSolver->clauses_string[c] << "\n";
	outfile << "\n";
	outfile.close(); 
}

// mark formula and all of its supersets as explored
void Master::block_up(MUS& formula){
	explorer->block_up(formula);
}

// mark formula and all of its subsets as explored
void Master::block_down(Formula formula){
        explorer->block_down(formula);
}

// experimental function
// allows output overapproximated MUSes 
bool Master::check_mus_via_hsd(Formula &f){
	Formula bot = explorer->get_bot_unexplored(f);
	int size_bot = count_ones(bot);
	int size_f = count_ones(f);
	bool result =  false;

	if(bot == f){
		result = true;
		cout << "guessed MUS" << endl;
	}

	else if((size_f - size_bot) < (dimension * mus_approx)){
		result = true;
		cout << "approximated MUS" << endl;
	}

	if(result){
		hsd_succ++;
		if(verify_approx){		
			Formula mus = satSolver->shrink(f);
			int f_size = count_ones(f);
			int mus_size = count_ones(mus);
			int distance = (f_size - mus_size);
			if(distance == 0){
				rightly_approximated_muses++;
				cout << "MUS approximation: success" << endl;				
			}
			else{
				overapproximated_muses++;
				total_difference_of_approximated_muses += distance;	
				cout << "MUS approximation: overapproximated, difference in size (over real MUS): " << distance << endl;
			}

			if(verbose){
				cout << "Total number of approximations: " << (rightly_approximated_muses + overapproximated_muses) << endl;
				cout << "Right approximations: " << rightly_approximated_muses << endl;
				cout << "Overapproximations: " << overapproximated_muses << endl;
				if(overapproximated_muses > 0){
					cout << "ratio right approximations / overapproximations: " << (rightly_approximated_muses / (float) overapproximated_muses) << endl;
					cout << "Average diff of overapproximation: " << (total_difference_of_approximated_muses / (float) overapproximated_muses) << endl;
				}	
			}
		}
	}
	return result;	
}

// check formula for satisfiability
// core and grow controls optional extraction of unsat core and model extension (replaces formula)
bool Master::is_valid(Formula &formula, bool core, bool grow){
	return satSolver->solve(formula, core, grow);
}

//verify if f is a MUS
void Master::validate_mus(Formula &f){
	if(is_valid(f))
		print_err("the mus is SAT");
	if(!explorer->checkValuation(f))
		print_err("this mus has been already explored");
	for(int l = 0; l < f.size(); l++)
		if(f[l]){
			f[l] = false;
			if(!is_valid(f))
				print_err("the mus has an unsat subset");	
			f[l] = true;
		}	
}

MUS& Master::shrink_formula(Formula &f, Formula crits){
	int f_size = count_ones(f);
	chrono::high_resolution_clock::time_point start_time = chrono::high_resolution_clock::now();
	if(crits.empty()) crits = explorer->critical;
	if(get_implies){ //get the list of known critical constraints	
		explorer->get_implies(crits, f);	
		cout << "criticals before rot: " << count_ones(crits) << endl;	
		if(criticals_rotation) satSolver->criticals_rotation(crits, f);
		cout << "criticals after rot: " << count_ones(crits) << endl;
		float ones_crits = count_ones(crits);		 
		if(f_size == ones_crits){ // each constraint in f is critical for f, i.e. it is a MUS 
			skipped_shrinks++; 
			muses.push_back(MUS(f, -1, muses.size(), f_size)); //-1 duration means skipped shrink
			return muses.back();
		}		
		if((ones_crits/f_size) > crits_treshold){
			if(!is_valid(crits, false, false)){ 
				skipped_shrinks++; 
				muses.push_back(MUS(crits, -1, muses.size(), f_size));//-1 duration means skipped shrink
				return muses.back();
			}
		}
	}

	Formula mus = satSolver->shrink(f, crits);
	chrono::high_resolution_clock::time_point end_time = chrono::high_resolution_clock::now();
	auto duration = chrono::duration_cast<chrono::microseconds>( end_time - start_time ).count() / float(1000000);
	muses.push_back(MUS(mus, duration, muses.size(), f_size));
	return muses.back();
}

//grow formula into a MSS
Formula Master::grow_formula(Formula &f){
	return satSolver->grow(f);
}


void Master::mark_MUS(MUS& f, bool block_unex){	
	if(validate_mus_c) validate_mus(f.bool_mus);		
	explorer->block_up(f, block_unex);

	chrono::high_resolution_clock::time_point now = chrono::high_resolution_clock::now();
	auto duration = chrono::duration_cast<chrono::microseconds>( now - initial_time ).count() / float(1000000);
        cout << "Found MUS #" << muses.size() <<  ", mus dimension: " << f.dimension;
	cout << ", checks: " << satSolver->checks << ", time: " << duration;
//	cout << ", sat rotated: " << explorer->sat_rotated;
	cout << ", unex sat: " << unex_sat << ", unex unsat: " << unex_unsat << ", criticals: " << explorer->criticals;
	cout << ", intersections: " << std::count(explorer->mus_intersection.begin(), explorer->mus_intersection.end(), true);
	cout << ", rotated MUSes: " << rotated_muses << ", explicit seeds: " << explicit_seeds;
	cout << ", union: " << std::count(explorer->mus_union.begin(), explorer->mus_union.end(), true) << ", dimension: " << dimension;
	cout << ", seed dimension: " << f.seed_dimension << ", duration: " << f.duration;
       	cout << ", used b: " << used_backbones_count << ", original b: " << original_backbones_count << endl;
	cout << ((original_backbones_count > 0)? (used_backbones_count/float(original_backbones_count)) : 0 ) << endl;

	if(output_file != "")
		write_mus_to_file(f);
}

void Master::enumerate(){
	initial_time = chrono::high_resolution_clock::now();
	cout << "running algorithm variant " << variant << endl;
	Formula whole(dimension, true);
	if(is_valid(whole))
		print_err("the input instance is satisfiable");

	switch (variant) {
		case 1:
		case 10:
		case 11:
		case 12:
			find_all_muses_duality_based_remus(Formula (dimension, true), Formula (dimension, false), 0);
			break;						
		case 2:
		case 21:
		case 22:
			find_all_muses_tome();
			break;
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
			marco_base();
			break;
		default:
			print_err("invalid algorithm chosen");
			break;
	}
	return;
}
