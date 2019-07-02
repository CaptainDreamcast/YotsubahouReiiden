#include "dialoghandler.h"

#include <prism/blitz.h>

#include "uihandler.h"
#include "level.h"
#include "boss.h"
#include "player.h"

#define DIALOG_Z 85

struct DialogStep {
	string mSpeaker;
	string mMood;
	string mText;
};

struct Speaker {
	string mName;
	int mIndex;
	map<string, int> mMoodAnimations;
};

struct Dialog {
	vector<string> mBeginMoods;
	vector<DialogStep> mSteps;
};

static Speaker& getSpeaker(int i);
static Speaker& getSpeaker(string name);

struct ActiveDialog{
	Dialog* mData;
	size_t mCurrentStep;

	int mTextBoxID;
	int mNameTextID;
	int mTextTextID;
	int mPortraitID[2];
	int mIsInIntro = 0;
	int mIntroNow;
	static ActiveDialog* mSelf;

	ActiveDialog(Dialog* tData) {
		mData = tData;
		mCurrentStep = 0;
		mSelf = this;

		for (int i = 0; i < 2; i++) mPortraitID[i] = addMugenAnimation(getMugenAnimation((i == 0) ? getPlayerAnimations() : getLevelAnimations(), getSpeaker(i).mMoodAnimations[mData->mBeginMoods[i]]), (i == 0) ? getPlayerSprites() : getLevelSprites(), makePosition(50 + i * 120, 200, DIALOG_Z));
		mTextBoxID = addMugenAnimation(getMugenAnimation(getUIAnimations(), 1000), getUISprites(), makePosition(96 + 16, 220, DIALOG_Z + 1));
		mNameTextID = addMugenTextMugenStyle("", makePosition(60, 174, DIALOG_Z + 2), makeVector3DI(4, 0, 1));
		mTextTextID = addMugenTextMugenStyle("", makePosition(60, 184, DIALOG_Z + 2), makeVector3DI(3, 0, 1));
		setMugenTextTextBoxWidth(mTextTextID, 105);
		setMugenTextScale(mTextTextID, 0.7);
		setMugenTextColorRGB(mTextTextID, 0, 0, 0);
		loadCurrentStep();
	}

	~ActiveDialog() {
		removeMugenAnimation(mTextBoxID);
		for (int i = 0; i < 2; i++) removeMugenAnimation(mPortraitID[i]);
		removeMugenText(mNameTextID);
		removeMugenText(mTextTextID);
	}

	static void introOver(void* tCaller) {
		(void)tCaller;
		mSelf->mIsInIntro = 2;
	}

	void loadCurrentStep() {
		if (mData->mSteps[mCurrentStep].mSpeaker == "intro") {
			mIsInIntro = 1;
			mIntroNow = 0;
			return;
		}

		auto& speaker = getSpeaker(mData->mSteps[mCurrentStep].mSpeaker);
		changeMugenText(mNameTextID, speaker.mName.data());
		changeMugenText(mTextTextID, mData->mSteps[mCurrentStep].mText.data());
		setMugenTextBuildup(mTextTextID, 1);

		if (mData->mSteps[mCurrentStep].mMood != "") {
			changeMugenAnimation(mPortraitID[speaker.mIndex], getMugenAnimation(getLevelAnimations(), speaker.mMoodAnimations[mData->mSteps[mCurrentStep].mMood]));
		}
	}

	int update() {
		int goToNext = 0;

		if (mIsInIntro) {
			if (mIsInIntro == 2) {
				mIsInIntro = 0;
				goToNext = 1;
			}
			else introBoss(introOver, mIntroNow);
			mIntroNow++;
		}
		else {
			if (hasPressedAFlank() || hasPressedB()) {
				if (!isMugenTextBuiltUp(mTextTextID)) {
					setMugenTextBuiltUp(mTextTextID);
				}
				else {
					goToNext = 1;
				}
			}
		}

		if (goToNext) {
			mCurrentStep++;
			if (mCurrentStep >= mData->mSteps.size()) {
				bossFinishedDialogCB();
				return 1;
			}
			else loadCurrentStep();
		}
		
		return 0;
		
	}

};

ActiveDialog* ActiveDialog::mSelf = nullptr;

struct {
	vector<Speaker> mSpeakers;
	Dialog mPreDialog;
	Dialog mPostDialog;

	unique_ptr<ActiveDialog> mActiveDialog;
} gDialogHandler;

static Speaker& getSpeaker(int i) {
	return gDialogHandler.mSpeakers[i];
}

static Speaker& getSpeaker(string name) {
	for (auto& speaker : gDialogHandler.mSpeakers) {
		if (stringEqualCaseIndependent(speaker.mName.data(), name.data())) return speaker;
	}
	logWarningFormat("Unable to find speaker %s", name.data());
	return gDialogHandler.mSpeakers[0];
}

static int isSpeakerGroup(string& tString) {
	return stringBeginsWithSubstringCaseIndependent(tString.data(), "speaker ");
}

typedef struct {
	Speaker* mSpeaker;

} SpeakerReadCaller;

static void loadSingleSpeakerGroup(void* tCaller, void* tData) {
	SpeakerReadCaller* caller = (SpeakerReadCaller*)tCaller;
	MugenDefScriptGroupElement* e = (MugenDefScriptGroupElement*)tData;
	caller->mSpeaker->mMoodAnimations[e->mName] = getMugenDefNumberVariableAsElement(e);
}

static void loadSpeakerGroup(MugenDefScriptGroup* tGroup) {
	Speaker speaker;

	char dummy[100], name[100];
	sscanf(tGroup->mName.data(), "%s %s", dummy, name);
	speaker.mName = string(name);
	speaker.mIndex = gDialogHandler.mSpeakers.size();

	SpeakerReadCaller caller;
	caller.mSpeaker = &speaker;
	list_map(&tGroup->mOrderedElementList, loadSingleSpeakerGroup, &caller);
	gDialogHandler.mSpeakers.push_back(speaker);
}

static int isDialogPreGroup(string& tString) {
	return stringBeginsWithSubstringCaseIndependent(tString.data(), "DialogPre");
}

static int isDialogPostGroup(string& tString) {
	return stringBeginsWithSubstringCaseIndependent(tString.data(), "DialogPost");
}

typedef struct {
	Dialog* mDialog;

} DialogReadCaller;

static void loadSingleDialogGroup(void* tCaller, void* tData) {
	DialogReadCaller* caller = (DialogReadCaller*)tCaller;
	MugenDefScriptGroupElement* e = (MugenDefScriptGroupElement*)tData;

	if (stringBeginsWithSubstringCaseIndependent(e->mName.data(), "start")) return;

	DialogStep step;

	string name = string(e->mName);
	string text = getSTLMugenDefStringVariableAsElement(e);

	auto moodBegin = name.find('[');
	if (moodBegin == name.npos) {
		step.mSpeaker = name;
		step.mMood = "";
	}
	else {
		step.mSpeaker = name.substr(0, moodBegin);
		auto moodEnd = name.find(']');
		step.mMood = name.substr(moodBegin + 1, moodEnd - moodBegin - 1);
	}
	step.mText = text;

	caller->mDialog->mSteps.push_back(step);
}

static void loadDialogGroup(MugenDefScriptGroup* tGroup, Dialog* tDialog) {
	DialogReadCaller caller;
	caller.mDialog = tDialog;

	auto beginMoods = getMugenDefStringVectorVariableAsGroup(tGroup, "start");
	for (int i = 0; i < beginMoods.mSize; i++) {
		tDialog->mBeginMoods.push_back(string(beginMoods.mElement[i]));
	}

	list_map(&tGroup->mOrderedElementList, loadSingleDialogGroup, &caller);
}

static void loadDialogFromScript(MugenDefScript& tScript) {
	MugenDefScriptGroup* current = tScript.mFirstGroup;
	while (current) {
		if (isSpeakerGroup(current->mName)) loadSpeakerGroup(current);
		else if (isDialogPreGroup(current->mName)) loadDialogGroup(current, &gDialogHandler.mPreDialog);
		else if (isDialogPostGroup(current->mName)) loadDialogGroup(current, &gDialogHandler.mPostDialog);

		current = current->mNext;
	}
}

static void loadDialogHandler(void* tData) {
	(void)tData;
	gDialogHandler.mSpeakers.clear();
	gDialogHandler.mActiveDialog = nullptr;
	gDialogHandler.mPreDialog = Dialog();
	gDialogHandler.mPostDialog = Dialog();

	stringstream ss;
	ss << "level/DIALOG" << getPlayerName() << getCurrentLevel() << ".txt";
	MugenDefScript script;
	loadMugenDefScript(&script, ss.str().data());
	loadDialogFromScript(script);
}

static void unloadDialogHandler(void* tData) {
	(void)tData;
	gDialogHandler.mSpeakers.clear();
	gDialogHandler.mActiveDialog = nullptr;
	gDialogHandler.mPreDialog = Dialog();
	gDialogHandler.mPostDialog = Dialog();
}

static void updateDialogHandler(void* tData) {
	(void)tData;
	if (!gDialogHandler.mActiveDialog) return;

	if (gDialogHandler.mActiveDialog->update()) {
		gDialogHandler.mActiveDialog = nullptr;
	}
}

ActorBlueprint getDialogHandler()
{
	return makeActorBlueprint(loadDialogHandler, unloadDialogHandler, updateDialogHandler);
}

void startPreDialog()
{
	gDialogHandler.mActiveDialog = make_unique<ActiveDialog>(&gDialogHandler.mPreDialog);
}

void startPostDialog()
{
	gDialogHandler.mActiveDialog = make_unique<ActiveDialog>(&gDialogHandler.mPostDialog);
}

void startCustomDialog()
{
}

int isDialogActive()
{
	return gDialogHandler.mActiveDialog != nullptr;
}
