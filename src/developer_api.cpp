#include "developer_api.h"

DeveloperAPI developerAPI;

void DeveloperAPI::begin()
{
}

void DeveloperAPI::update()
{
}

String DeveloperAPI::getName()
{
    return "Sudev Kuchhal";
}

String DeveloperAPI::getRole()
{
    return "Embedded Systems";
}

String DeveloperAPI::getGitHub()
{
    return "sudevkuchhal";
}

String DeveloperAPI::getLinkedIn()
{
    return "linkedin.com/in/sudevkuchhal";
}

String DeveloperAPI::getProject()
{
    return "LumiOS X";
}

// --------------------
// New Functions
// --------------------

int DeveloperAPI::getRepos()
{
    return 12;   // Change later with real GitHub API
}

int DeveloperAPI::getStars()
{
    return 27;   // Change later with real GitHub API
}

String DeveloperAPI::getStatus()
{
    return "Building LumiOS X";
}