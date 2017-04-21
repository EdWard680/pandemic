#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <iterator>
#include <cctype>
#include <algorithm>

enum command_t {HELP = 0, DRAW, INFECT, EPIDEMIC, EPIDEMIC_STATS,
				INFECT_STATS, CARD_STATS, TURNS_LEFT, N_COMMANDS};
command_t parse_command(const std::string &command);

const char * commands[] = {
	[HELP] = "help",
	[DRAW] = "draw",
	[INFECT] = "infect",
	[EPIDEMIC_STATS] = "epidemic_stats",
	[EPIDEMIC] = "epidemic",
	[INFECT_STATS] = "infect_stats",
	[CARD_STATS] = "card_stats",
	[TURNS_LEFT] = "turns"
};

enum color_t {YELLOW = 0, RED, BLUE, BLACK, EVENT, N_COLORS};
color_t to_color(const std::string &str);
std::string color_to_string(color_t color);

const char * colors[] = {
	[YELLOW] = "yellow",
	[RED] = "red",
	[BLUE] = "blue",
	[BLACK] = "black",
	[EVENT] = "event"
};

class LazyString: public std::string
{
public:
	using std::string::string;
	LazyString(const std::string &s): std::string(s) {}
};

bool operator== (const LazyString &lhs, const LazyString &rhs)
{
	for(int i = 0; i < std::min(lhs.size(), rhs.size()); i++)
		if(tolower(lhs[i]) != tolower(rhs[i])) return false;
	
	return true;
}

bool operator< (const LazyString &lhs, const LazyString &rhs)
{
	for(int i = 0; i < std::min(lhs.size(), rhs.size()); i++)
	{
		if(tolower(lhs[i]) < tolower(rhs[i])) return true;
		else if(tolower(lhs[i]) > tolower(rhs[i])) return false;
	}
	
	return false;
}

using deck_t = std::map<LazyString, color_t>;

int run(std::istream &in);
deck_t load_cities(const std::string &filename);

template<class T>
std::istream& operator>> (std::istream &in, std::vector<T> &v)
{
	v = std::vector<T>(std::istream_iterator<T>(in), std::istream_iterator<T>());
	return in;
}

template<class T>
std::ostream& operator<< (std::ostream &out, std::vector<T> &v)
{
	for(auto i : v)
		out << i;
	
	return out;
}

std::vector<std::string> get_cards(std::istream &in)
{
	std::vector<std::string> ret;
	std::string args;
	std::getline(in, args);
	std::istringstream iss(args);
	iss >> ret;
	return ret;
}

int run(std::istream& in)
{
	auto input_with_default = [&in](const std::string &message, auto def)
	{
		std::cout << message << " [" << def << "]: ";
		std::string temp;
		std::getline(in, temp);
		if(temp.empty()) return def;
		std::istringstream iss(temp);
		decltype(def) ret;
		iss >> ret;
		return ret;
	};
	
	std::string city_file =
		input_with_default("Input cities file", std::string("cities.txt"));
	
	std::vector<std::string> events = 
		input_with_default("Select funded events", std::vector<std::string>());
	
	int initial_draws = input_with_default("Select number of initial draws", 8);
	int epidemics = input_with_default("Select number of epidemics", 5);
	
	auto cities = load_cities(city_file);
	std::cout << cities.size() << " cities loaded" << std::endl;
	
	std::vector<decltype(cities)> infection_deck{cities};
	decltype(cities) infection_discard;
	
	for(auto event : events)
		cities.insert(std::make_pair(event, EVENT));
	
	int cards_per_epidemic = (cities.size() - initial_draws + epidemics) / epidemics;
	int big_stacks = (cities.size() - initial_draws + epidemics) - 
					cards_per_epidemic * epidemics;
	int n_draws = -8;
	int current_epidemics = 0;
	
	auto player_deck(cities);
	decltype(player_deck) player_drawn;
	
	auto ambig = [cities](const std::string &draw)
	{
		auto range = cities.equal_range(draw);
		int ret = 0;
		if((ret = std::distance(range.first, range.second)) > 1)
		{
			// ambiguous city
			std::cout << draw << " was ambiguous. Could be: ";
			for(auto it = range.first; it != range.second; ++it)
				std::cout << it->first << ", ";
			std::cout << "\b\b." << std::endl;
		}
		else if(ret == 0)
		{
			std::cout << draw << " is an invalid card. " << std::endl;
		}
		
		return ret;
	};
	
	
	std::string command;
	std::cout << "(pandemic - don't forget to record initial draws and infects) ";
	while(in >> command)
	{
		switch(parse_command(command))
		{
		case INFECT:
			{
				std::vector<std::string> infects = get_cards(in);
				
				for(const auto &infect : infects)
				{
					if(ambig(infect) != 1) continue;
					
					if(auto card = infection_deck.back().find(infect);
					   card != infection_deck.back().end())
					{
						std::cout << "Infecting: " << card->first << std::endl;
						infection_discard.insert(std::move(*card));
						infection_deck.back().erase(card);
						if(infection_deck.back().size() == 0)
							infection_deck.pop_back();
					}
					else
					{
						std::cout << "error: could not find " << infect;
						std::cout << " in any deck." << std::endl;
					}
				}
				
				break;
			}
		case DRAW:
			{
				std::vector<std::string> draws = get_cards(in);
				bool epidemic = false;
				
				for(const auto &draw : draws)
				{
					if(LazyString(draw) == LazyString("epidemic"))
					{
						if(epidemic)  // in case user does double epidemic
						{
							current_epidemics++;
							n_draws++;
						}
						
						epidemic = true;
						continue;
					}
					
					if(ambig(draw) != 1) continue;
					
					if(auto card = player_deck.find(draw);
						card != player_deck.end())
					{
						std::cout << "Drew " << card->first << std::endl;
						player_drawn.insert(std::move(*card));
						player_deck.erase(card);
						n_draws++;
					}
					else
					{
						std::cout << "error: " << player_drawn.find(draw)->first;
						std::cout << " was already drawn" << std::endl;
					}
				}
				
				std::cout << n_draws << " draws so far." << std::endl;
				
				if(!epidemic)
					break;
			}
		case EPIDEMIC:
			current_epidemics++;
			n_draws++;
			std::cout << "Epidemic " << current_epidemics << std::endl;
		case EPIDEMIC_STATS:
			{
				int safe_phase = 0;
				if(current_epidemics < big_stacks)
					safe_phase = current_epidemics * (cards_per_epidemic + 1);
				else
					safe_phase = big_stacks * (cards_per_epidemic + 1) +
						(current_epidemics - big_stacks) * cards_per_epidemic;
				
				int next_phase = safe_phase + 
							cards_per_epidemic + (current_epidemics < big_stacks);
				
				if(n_draws + 2 <= safe_phase)
				{
					std::cout << "No epidemics for " << safe_phase - n_draws;
					std::cout << " more draws. (" << (safe_phase - n_draws) / 2;
					std::cout << " turns)" << std::endl;
				}
				else if(n_draws + 2 <= next_phase)
				{
					std::cout << "Epidemic is in the next " << next_phase - n_draws;
					std::cout << " draws (" << 200.0 / (next_phase - n_draws);
					std::cout << "% chance to draw this turn) " << std::endl;
				}
				else if(n_draws + 1 == next_phase)
				{
					std::cout << "Epidemic will be the next card drawn, ";
					std::cout << "followed by a 1/";
					std::cout << (cards_per_epidemic + (current_epidemics < big_stacks));
					std::cout << " chance of drawing another after" << std::endl;
				}
				
				break;
			}
		default:
			std::cout << "error: invalid command" << std::endl;
			break;
		}
		
		std::cout << "(pandemic) ";
	}
	
	return 0;
}

deck_t load_cities(const std::string &filename)
{
	std::ifstream in(filename);
	deck_t ret;
	
	for(std::istream_iterator<std::string> it(in);
		it != std::istream_iterator<std::string>(); ++it)
	{
		auto key = *it++;
		ret.insert(std::make_pair(std::move(key), to_color(*it)));
	}
	
	return ret;
}

command_t parse_command(const std::string &command)
{
	for(int i = 0; i < N_COMMANDS; i++)
		if(command == commands[i]) return static_cast<command_t>(i);
	
	return N_COMMANDS;
}

color_t to_color(const std::string &str)
{
	for(int i = 0; i < N_COLORS; i++)
		if(str == colors[i]) return static_cast<color_t>(i);
	
	return N_COLORS;
}

std::string color_to_string(color_t color)
{
	return colors[static_cast<int>(color)];
}

int main(int argc, char *argv[])
{
	if(argc > 1)
	{
		std::ifstream in(argv[1]);
		return run(in);
	}
	else
		return run(std::cin);
}
