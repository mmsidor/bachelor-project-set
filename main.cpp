#include<iostream>
#include<array>
#include<vector>
#include<set>
#include<random>
#include<algorithm>
#include<string>
#include<map>
#include<thread>
#include<mutex>
#include<cassert>
#include<climits>
const int mod = 3;
const int dim = 4;
const int table_size = 12;
const std::string colors[3] = {"purple", "red", "green"};
const std::string numbers[3] = {"one", "two", "three"};
const std::string shapes[3] = {"ovals", "squiggles", "diamonds"};
const std::string fills[3] = {"empty", "striped", "filled"};
std::mutex stats_mutex;
enum Selection{
	RANDOM,
	QUASITHRIFTY,
	THRIFTY,
	WEIGHTEDTHRIFTY

};


struct F3{
	int x;
	F3(): x(0){}
	F3(int xx): x(xx){}
	F3 operator+(F3 b)const{ return F3((x+b.x)%mod);}
	F3 operator-(F3 b)const{ return F3((x-b.x+mod)%mod);}
	F3 operator-()const{return F3((mod-x)%mod);}
	bool operator==(F3 b)const{return x==b.x;}
};

struct Card{
	std::array<F3,4>a;
	Card(std::array<F3,4>aa){
		a = aa;
	}
	Card(int x){
		a = {x%3, (x/3)%3, (x/9)%3, (x/27)%3};
	}
	std::string human_card(){
		return numbers[a[0].x] + ' ' + colors[a[1].x] + ' ' + fills[a[2].x] + ' ' + shapes[a[3].x];
	}

	int compval()const{return a[3].x + 3*a[2].x + 9*a[1].x + 27*a[0].x;}
	Card operator+(Card C)const {return Card({a[0]+C.a[0], a[1]+C.a[1], a[2]+C.a[2], a[3]+C.a[3]});}
	Card operator-()const {return Card({-a[0], -a[1], -a[2], -a[3]});}
	bool operator<(Card C)const {return compval() < C.compval();}
	bool operator==(Card C)const{return a[0]==C.a[0] && a[1] == C.a[1] && a[2] == C.a[2] && a[3] == C.a[3];}
};

bool intersect(std::vector<Card>A, std::vector<Card>B){
	bool res = false;
	for(int i = 0; i < 3; i++){
		for(int j = 0; j < 3; j++){
			if(A[i] == B[j])res = true;
		}
	}
	return res;
}

struct Game{
	int seed;
	bool finished;
	bool verbose;
	std::mt19937 rng;
	std::vector<Card>deck;
	std::set<Card>table;
	std::set<std::vector<Card>>existing_sets;
	std::set<std::vector<Card>>possible_sets;
	Selection s;
	Game(int sd, bool verb=false){//initialize a game
		finished = false;
		seed = sd;
		verbose=verb;	
		rng.seed(seed);
		s = THRIFTY;
		for(int i = 0; i < 81; i++){
			Card C1 = Card(i);
			for(int j = i+1; j < 81; j++){
				Card C2 = Card(j);
				for(int k = j + 1; k < 81; k++){
					Card C3 = Card(k);
					if(-(C1 + C2) == C3){
						possible_sets.insert({C1, C2, C3});
					}
				}
			}
		}

		for(int i = 0; i < 81; i++){
			deck.push_back(Card({i%3, (i/3)%3, (i/9)%3, (i/27)%3}));
		}
		std::shuffle(deck.begin(), deck.end(), rng);
	}

	std::vector<Card> random_choose(){
		auto it = existing_sets.begin();
		int random_choice = rng()%(int)existing_sets.size();
		std::advance(it, random_choice);
		return *it;
	}

	int calculate_impact(std::vector<Card>set, const std::vector<std::vector<Card>>& possible = std::vector<std::vector<Card>>(0)){
		int res = 0;
		if(possible.empty()){
			for(auto poset : possible_sets){
				if(intersect(set, poset)){
					res++;
				}
			}
		} else{
			for(auto poset : possible){
				if(intersect(set, poset))
					res++;
			}
		}
		return res;
	}
	std::vector<std::vector<Card>>find_all_min_impact(){
		int min_impact = 10000;
		std::vector<int>impact(existing_sets.size());
		int it = 0;
		for(auto set : existing_sets){
			int imp = calculate_impact(set);
			impact[it++] = imp;
			min_impact =  std::min(min_impact, imp);
		}
		std::vector<std::vector<Card>>res;
		it = 0;
		for(auto set: existing_sets){
			if(impact[it++] == min_impact)//optimize later
				res.push_back(set);
		}
		return res;
	}
	std::vector<Card>quasi_thrifty_choose(){
		auto min_impact_sets = find_all_min_impact();
		assert(!min_impact_sets.empty());
		return min_impact_sets[0];
	}

	std::vector<Card>thrifty_choose(){
		auto min_impact_sets = find_all_min_impact();
		auto chosen_set = min_impact_sets[0];
		if(min_impact_sets.size() == 1)return chosen_set;
		int lowest_imp = 10000;
		for(auto set: min_impact_sets){
			int imp = 0;
			std::vector<std::vector<Card>>removed;
			std::vector<std::vector<Card>> new_possible_sets;
			for(auto poset : possible_sets){
				if(intersect(poset, set)){
					removed.push_back(poset);
				}
			}
			std::set_difference(possible_sets.begin(), possible_sets.end(), removed.begin(), removed.end(), std::inserter(new_possible_sets, new_possible_sets.begin()));
			for(auto poset: new_possible_sets){
				imp += calculate_impact(poset, new_possible_sets);
			}

			if(imp < lowest_imp){
				lowest_imp = imp;
				chosen_set = set;
			}
		}
		return chosen_set;
	}

	std::vector<Card>weighted_thrifty_choose(){
		auto min_impact_sets = find_all_min_impact();
		auto chosen_set = min_impact_sets[0];
		if(min_impact_sets.size() == 1)return chosen_set;
		long double lowest_imp = 100000.0;
		for(auto set: min_impact_sets){
			long double imp = 0;
			std::vector<std::vector<Card>>removed;
			std::vector<std::vector<Card>> new_possible_sets;
			for(auto poset : possible_sets){
				if(intersect(poset, set)){
					removed.push_back(poset);
				}
			}
			std::set_difference(possible_sets.begin(), possible_sets.end(), removed.begin(), removed.end(), std::inserter(new_possible_sets, new_possible_sets.begin()));
			for(auto poset: new_possible_sets){
				int needed = 3;
				for(auto table_card : table){
					for(auto card : poset){
						if(table_card == card)needed--;
					}
				}
				int k = deck.size();
				double prob = 1;
				if(k == 0){
					if(needed > 0)prob = 0;
				} else {
					if(needed == 3){
						prob = 6.0/(k*(k-1)*(k-2));
					} else if(needed == 2){
						prob = 6.0/(k*(k-1));
					} else if(needed = 1){
						prob = 3.0/k;
					} else prob = 1;
				}
				imp += prob * calculate_impact(poset, new_possible_sets);
			}

			if(imp < lowest_imp){
				lowest_imp = imp;
				chosen_set = set;
			}
		}
		return chosen_set;	
	}

	std::vector<Card>choose(){
		if(s == RANDOM)return random_choose();
		if(s == QUASITHRIFTY)return quasi_thrifty_choose();
		if(s == THRIFTY)return thrifty_choose();
		if(s == WEIGHTEDTHRIFTY)return weighted_thrifty_choose();
		return random_choose();
	}


	bool next(){//makes next move, returns true if not possible to do anything
		if(deck.empty() && existing_sets.empty())return true;// end game condition
		while((table.size() < table_size || existing_sets.empty()) && !deck.empty())deal(3); // deal cards
		if(!existing_sets.empty()){
			erase(choose());//remove a chosen set
		}
		return false;
	}
	void deal(int n){//deal n cards
		while(n--)deal();
	}
	void deal(){//deal one card and update existing SETs
		Card x = deck.back();
		deck.pop_back();
		for(auto y: table){
			if(table.count(-(x+y))){
				std::vector<Card>set = {x, y, -(x+y)};
				std::sort(set.begin(), set.end());
				existing_sets.insert(set);
			}
		}
		table.insert(x);
	}
	
	void erase(std::vector<Card>removed_set){//remove a SET from the table
		if(verbose){
			std::cout << "Erasing SET:\n";
			for(Card x : removed_set)std::cout << x.human_card() << '\n';
		}
		for(Card x : removed_set)table.erase(x);
		for(auto it = existing_sets.begin(); it != existing_sets.end(); ){
			auto itt = it++;
			auto set = *itt;
			for(Card x : removed_set){
				if(x == set[0] or x == set[1] or x == set[2]){
					existing_sets.erase(set);
					break;
				}
			}
		}
		for(auto it = possible_sets.begin(); it != possible_sets.end(); ){
			auto itt = it++;
			auto set = *itt;
			for(Card x : removed_set){
				if(x == set[0] or x == set[1] or x == set[2]){
					possible_sets.erase(set);
					break;
				}
			}
		}

	}
	int play(){
		while(!finished){
			finished = next();
		}
		return table.size();
	}
};


void simulate_range(int start, int end, std::map<int, int>& local_stats) {
    std::map<int, int> private_stats;
    for (int i = start; i < end; i++) {
        Game G(i);
        private_stats[G.play()]++;
    }

    std::lock_guard<std::mutex> lock(stats_mutex);
    for (const auto& [key, value] : private_stats) {
        local_stats[key] += value;
    }
}


int main(){
	std::map<int,int>stats;
	std::vector<std::thread>threads;
	const int num_simulations = 100;
	const int num_threads = std::thread::hardware_concurrency();
    int chunk_size = num_simulations/num_threads;
	for (int t = 0; t < num_threads; t++) {
        int start = t * chunk_size + 1;
        int end = (t == num_threads - 1) ? num_simulations + 1 : start + chunk_size;
		threads.emplace_back(simulate_range, start, end, std::ref(stats));
	}

    for (auto& th : threads) {
        th.join();
    }
	int total = 0;
    for (const auto& [key, value] : stats) {
        total += value;
		std::cout << "Outcome " << key << ": " << value << " times\n";
    }
	std::cout << "Total: " << total << '\n';

}
