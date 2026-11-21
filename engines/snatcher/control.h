#ifndef SNATCHER_CONTROL_H
#define SNATCHER_CONTROL_H
#include <cstdint>
#include <string>
#include <vector>
namespace Snatcher {
enum ActivationDecision {kActivationError,kActivationEntered,kActivationSkipped};
enum ChoiceDecision { kChoiceOffered, kChoiceSkipped, kChoiceEntered, kChoicePending, kChoiceError };
struct ControlSnapshot {
 std::vector<uint16_t> available,selected,activations,history;
 uint16_t current,pending;bool activationMode;
 ControlSnapshot():current(0),pending(0xffff),activationMode(false){}
};
bool validateControlSnapshot(const ControlSnapshot& state,std::string& error);
// Platform-free scenario choice/control state. Text references are identities;
// UI ordinals are resolved only by respond(). No input or playback loop lives here.
class ControlState {
public:
 explicit ControlState(uint16_t initialCursor=0);
 bool gate() const;
 ChoiceDecision beginChoice(uint16_t textReference,std::string &error);
 bool endChoice(std::string &error);
 bool respond(unsigned oneBasedChoice,std::string &error);
 bool cancel(std::string &error);
 bool emptyPass(std::string &error);
 ActivationDecision enterActivation(std::string &error);
 bool leaveActivation(std::string &error);
 bool requestContinuation(uint16_t cursor,std::string &error);
 bool replaceContinuation(uint16_t cursor,std::string &error);
 bool applyContinuation(std::string &error);
 // Original traversal preparation clears activation mode, not selected path.
 bool beginPass(std::string &error);
 bool snapshot(ControlSnapshot& state,std::string& error)const;
 bool restore(const ControlSnapshot& state,std::string& error);
 const std::vector<uint16_t>&availableChoices()const{return _available;}
 const std::vector<uint16_t>&selectedPath()const{return _selected;}
 const std::vector<uint16_t>&activationDepths()const{return _activations;}
 const std::vector<uint16_t>&cursorHistory()const{return _history;}
 unsigned depth()const{return _depth;}
 bool activationMode()const{return _activationMode;}
 uint16_t currentCursor()const{return _current;}
 bool hasPendingContinuation()const{return _pending!=0xffff;}
 uint16_t pendingCursor()const{return _pending;}
private:
 std::vector<uint16_t> _available,_selected,_activations,_history;
 std::vector<unsigned> _choiceScopes;
 uint16_t _current,_pending;unsigned _depth;bool _activationMode;
 unsigned activationFloor()const;
 bool atBoundary(std::string &error)const;
};
}
#endif
