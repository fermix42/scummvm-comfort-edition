/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_SCENARIO_H
#define SNATCHER_SCENARIO_H
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace Snatcher {
struct Field { uint8_t type; uint32_t offset; uint16_t value; };
struct Node {
 uint32_t offset, end; uint8_t opcode;
 std::vector<Field> fields; std::vector<uint32_t> children;
 bool hasReference; uint16_t reference;
 Node() : offset(0),end(0),opcode(0),hasReference(false),reference(0) {}
};
struct Schema { uint8_t field[3]; Schema(uint8_t a=0,uint8_t b=0,uint8_t c=0) {field[0]=a;field[1]=b;field[2]=c;} };
struct Scenario {
 std::vector<Node> nodes; std::map<uint32_t,size_t> byOffset;
 size_t size, codeBytes, textBytes, fillBytes, unknownCode, unknownText, textRecords;
 std::string unresolved;
 Scenario() : size(0),codeBytes(0),textBytes(0),fillBytes(0),unknownCode(0),unknownText(0),textRecords(0) {}
 const Node *at(uint32_t offset) const;
};
bool schemaFor(uint8_t opcode, Schema &out);
bool parseTerm(const uint8_t *data,size_t size,size_t start,size_t limit,std::vector<Node>& nodes,size_t& end,std::string& error);
bool parseScenario(const uint8_t *data,size_t size,Scenario &out,std::string &error);
bool validateLocalTargets(const Scenario &scenario,std::string &error);
bool validateCrossTargets(const std::vector<Scenario>& scenarios,size_t& count,std::string& error);
bool isPureExpression(uint8_t opcode);
bool readPacked(const std::vector<uint8_t>& state,uint16_t selector,uint16_t& value,std::string& error);
bool writePacked(std::vector<uint8_t>& state,uint16_t selector,uint16_t value,std::string& error);
// Diagnostics observe the actual successful read; no duplicate evaluation.
typedef void (*StateReadObserver)(void *context,uint32_t node,uint16_t selector,uint16_t value);
typedef bool (*ExpressionResolver)(void *context,const Node& node,uint16_t& value,std::string& error);
bool evaluate(const Scenario& scenario,uint32_t offset,const std::vector<uint8_t>& state,uint16_t& value,std::string& error,StateReadObserver observer=nullptr,void *context=nullptr,ExpressionResolver resolver=nullptr);
}
#endif
