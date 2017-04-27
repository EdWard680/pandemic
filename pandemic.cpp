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
#include <readline/readline.h>
#include <csignal>

#include "Console.hpp"

using namespace CppReadline;

enum command_t {HELP = 0, DRAW, INFECT, EPIDEMIC, EPIDEMIC_STATS,
				INFECT_STATS, CARD_STATS, N_COMMANDS};
command_t parse_command(const std::string &command);

const char * commands[] = {
	[HELP] = "help",
	[DRAW] = "draw",
	[INFECT] = "infect",
	[EPIDEMIC_STATS] = "epidemic_stats",
	[EPIDEMIC] = "epidemic",
	[INFECT_STATS] = "infect_stats",
	[CARD_STATS] = "card_stats"
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

std::ostream& operator<< (std::ostream &out, 
						  const std::pair<const LazyString, color_t> &card)
{
	static const char * color_codes[] = {
		[YELLOW] = "\e[43m",
		[RED] = "\e[41m",
		[BLUE] = "\e[44m",
		[BLACK] = "\e[100m",
		[EVENT] = "\e[42m",
		[N_COLORS] = "\e[49m"
	};
	
	return out << color_codes[card.second] << card.first << color_codes[N_COLORS];
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

/* Main function. 
 * Initializes a readline console and runs it through infinite loop
 */
int run(const std::string &script)
{
	std::istream &in = std::cin;
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
	
	int total_cards = cities.size() - initial_draws + epidemics;
	int cards_per_epidemic = total_cards / epidemics;
	int big_stacks = total_cards - cards_per_epidemic * epidemics;
	int n_draws = -8;
	int current_epidemics = 0;
	
	auto player_deck(cities);
	decltype(player_deck) player_drawn;
	
	// used to check ambiguous cards
	auto ambig = [&cities](const std::string &draw)
	{
		auto range = cities.equal_range(draw);
		int ret = 0;
		if((ret = std::distance(range.first, range.second)) > 1)
		{
			// ambiguous city
			std::cout << draw << " was ambiguous. Could be: ";
			for(auto it = range.first; it != range.second; ++it)
				std::cout << *it << ", ";
			std::cout << "\b\b." << std::endl;
		}
		else if(ret == 0)
		{
			std::cout << draw << " is an invalid card. " << std::endl;
		}
		
		return ret;
	};
	
	// handles completion
	Console::registerArgCompletionFunction([&cities](const std::string &text)
	{
		auto range = cities.equal_range(text);
		std::vector<std::string> ret;
		if(std::distance(range.first, range.second) > 1)
			ret.push_back("");
		for(auto i = range.first; i != range.second; ++i)
		{
			ret.push_back(i->first);
		}
		
		return ret;
	});
	
	/* Creates console. Registering lambdas to each command.
	 */
	Console console("(pandemic) ");
	rl_bind_key(24, [](int count, int key)
	{
		std::string line("un");
		line += rl_line_buffer;
		rl_replace_line(line.c_str(), 0);
		return 0;
	});
	
	console.registerCommand("infect", [&](const Console::Arguments &args)
	{
		Console::Arguments infects(args.begin() + 1, args.end());
		for(const auto &infect : infects)
		{
			if(ambig(infect) != 1) continue;
			
			if(auto card = infection_deck.back().find(infect);
				card != infection_deck.back().end())
			{
				std::cout << "Infecting: " << *card << std::endl;
				infection_discard.insert(std::move(*card));
				infection_deck.back().erase(card);
				if(infection_deck.back().size() == 0)
					infection_deck.pop_back();
			}
			else
			{
				std::cout << "error: " << infect;
				std::cout << " is not at the top of the deck." << std::endl;
			}
		}
		return 0;
	});
	
	console.registerCommand("uninfect", [&](const Console::Arguments &args)
	{
		Console::Arguments infects(args.begin() + 1, args.end());
		for(const auto &infect : infects)
		{
			if(ambig(infect) != 1) continue;
			
			if(auto card = infection_discard.find(infect);
				card != infection_discard.end())
			{
				std::cout << "Uninfecting: " << *card << std::endl;
				infection_deck.back().insert(std::move(*card));
				infection_discard.erase(card);
				if(infection_discard.size() == 0)
					infection_deck.pop_back();
			}
			else
			{
				std::cout << "error: " << infect;
				std::cout << " is not in the discard pile." << std::endl;
			}
		}
		return 0;
	});
	
	console.registerCommand("draw", [&](const Console::Arguments &args)
	{
		Console::Arguments draws(args.begin() + 1, args.end());
		for(const auto &draw : draws)
		{
			if(ambig(draw) != 1) continue;
			
			if(auto card = player_deck.find(draw);
				card != player_deck.end())
			{
				std::cout << "Drew " << *card << std::endl;
				player_drawn.insert(std::move(*card));
				player_deck.erase(card);
				n_draws++;
			}
			else
			{
				std::cout << "error: " << *player_drawn.find(draw);
				std::cout << " was already drawn" << std::endl;
			}
		}
		
		std::cout << n_draws << " draws so far." << std::endl;
		
		return 0;
	});
	
	console.registerCommand("undraw", [&](const Console::Arguments &args)
	{
		Console::Arguments draws(args.begin() + 1, args.end());
		for(const auto &draw : draws)
		{
			if(ambig(draw) != 1) continue;
			
			if(auto card = player_drawn.find(draw);
				card != player_drawn.end())
			{
				std::cout << "Undrew " << *card << std::endl;
				player_deck.insert(std::move(*card));
				player_drawn.erase(card);
				n_draws--;
			}
			else
			{
				std::cout << "error: " << *player_deck.find(draw);
				std::cout << " hasn't been drawn yet" << std::endl;
			}
		}
		
		std::cout << n_draws << " draws so far." << std::endl;
		
		return 0;
	});
	
	console.registerCommand("epidemic", [&](const Console::Arguments &infections)
	{
		current_epidemics++;
		n_draws++;
		std::cout << "Epidemic " << current_epidemics << std::endl;
		std::string infection;
		if(infections.size() > 1)
			infection = infections[1];
		
		while(true)
		{
			while(infection.empty() || ambig(infection) != 1)
			{
				std::cout << "(infect from bottom) ";
				in >> infection;
			}
			
			if(infection_deck.front().count(infection) == 0)
				std::cout << "error: That card is not in the bottom deck" << std::endl;
			else
				break;
		}
		
		auto card = infection_deck.front().find(infection);
		std::cout << "Infecting " << *card << std::endl;
		infection_discard.insert(std::move(*card));
		infection_deck.push_back(std::move(infection_discard));
		infection_discard.clear();
		
		return console.executeCommand("epidemic_stats");
	});
	
	console.registerCommand("unepidemic", [&](const Console::Arguments &infections)
	{
		current_epidemics--;
		n_draws--;
		std::cout << "Epidemic " << current_epidemics << std::endl;
		std::string infection;
		if(infections.size() > 1)
			infection = infections[1];
		
		while(true)
		{
			if(!infection.empty() && infection_deck.back().count(infection) == 0)
				std::cout << "error: That card is not on top of the infect deck" << std::endl;
			else if(!infection.empty())
				break;
			
			do
			{
				std::cout << "uninfect from bottom: ";
				in >> infection;
			} while(infection.empty() || ambig(infection) != 1);
		}
		
		auto card = infection_deck.back().find(infection);
		std::cout << "Uninfecting " << *card << std::endl;
		infection_deck.front().insert(std::move(*card));
		std::copy(infection_deck.back().begin(), infection_deck.back().end(),
			std::inserter(infection_discard, infection_discard.begin()));
		infection_deck.pop_back();
		
		return console.executeCommand("epidemic_stats");
	});
	
	console.registerCommand("epidemic_stats", [&](const Console::Arguments&)
	{
		std::cout << "Epidemics so far: " << current_epidemics << std::endl;
		std::cout << "Draws left: " << total_cards - n_draws << std::endl;
		std::cout << "Turns left: " << (total_cards - n_draws) / 2 << std::endl;
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
		
		return 0;
	});
	
	console.registerCommand("infect_stats", [&](const Console::Arguments&)
	{
		std::cout << "Infection Discard: {";
		for(auto i : infection_discard)
			std::cout << i << ", ";
		std::cout << "}\n" << std::endl;
		
		std::cout << "The next infections are:" << std::endl;
		for(auto i = infection_deck.rbegin(); i != infection_deck.rend(); ++i)
		{
			std::cout << "{";
			for(auto j : *i)
				std::cout << j << ", ";
			std::cout << "}\n" << std::endl;
		}
		
		return 0;
	});
	
	console.registerCommand("card_stats", [&](const Console::Arguments&)
	{
		std::cout << "Cards left in player deck: {";
		int counts[N_COLORS] = {0};
		for(auto card : player_deck)
		{
			++counts[card.second];
			std::cout << card << ", ";
		}
		std::cout << "}\n" << std::endl;
		
		for(int color = 0; color < N_COLORS; color++)
		{
			std::cout << std::make_pair(std::to_string(counts[color]) + " " +
										std::string(colors[color]),
										color_t(color));
			if(color < N_COLORS - 1)
				std::cout << ", ";
		}
		std::cout << std::endl;
		return 0;
	});
	
	//std::cout << "(pandemic - don't forget to record initial draws and infects) ";
	
	
	while(console.readLine() != Console::Quit)
	{
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
	return run(argc > 1? argv[1]:"");
}
