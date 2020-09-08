#pragma once

#include <boost/lockfree/queue.hpp>

#include <nlohmann/json.hpp>

#include <iostream>
#include <variant>

// for convenience
using json = nlohmann::json;

struct Lightbulb{
  Lightbulb(std::string name) : name_(std::move(name)){}

  void set_brightness(unsigned val) {
    std::cout << "Lightbulb(" << name_ << ") set brightness to " << val << '\n';
  }

  void set_color(unsigned r, unsigned g, unsigned b) {
    std::cout << "Lightbulb(" << name_ << ") set color to RGB("
              << r << ',' << g << ',' << b << ")\n";
  }
  std::string name_ = "";
};

namespace cmd {

struct SetBrightness {
  unsigned val = 0;
};

struct SetColor {
  unsigned r = 0;
  unsigned g = 0;
  unsigned b = 0;
};

using Command = std::variant<SetBrightness, SetColor>;
using CommandQueue = boost::lockfree::queue<cmd::Command>;

struct CommandExecutor{
  CommandExecutor(Lightbulb& bulb) : bulb_(bulb){};

  void operator()(const SetBrightness& cmd){
    bulb_.set_brightness(cmd.val);
  }

  void operator()(const SetColor& cmd){
    bulb_.set_color(cmd.r, cmd.g, cmd.b);
  }

private:
  Lightbulb& bulb_;
};

inline
void to_json(json& j, const SetBrightness& cmd) {
  j = json{{"brightness", cmd.val}};
}

inline
void from_json(const json& j, SetBrightness& cmd) {
  j.at("brightness").get_to(cmd.val);
}

inline
void to_json(json& j, const SetColor& cmd) {
  j = json{{"red", cmd.r},{"green", cmd.g},{"red", cmd.b}};
}

inline
void from_json(const json& j, SetColor& cmd) {
  j.at("red").get_to(cmd.r);
  j.at("green").get_to(cmd.g);
  j.at("blue").get_to(cmd.b);
}

inline
Command deserialize(const json& j){
  Command ret;
  const auto type = j.at("command_type").get<std::string>();
  if (type == "set_brightness") {
    ret = j.at("command_arguments").get<SetBrightness>();
  } else if (type == "set_color") {
    ret = j.at("command_arguments").get<SetColor>();
  } else {
    throw std::runtime_error("Could not deserialize json command " + j.dump());
  }
  return ret;
}

} // namespace cmd
