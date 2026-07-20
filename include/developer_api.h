#ifndef DEVELOPER_API_H
#define DEVELOPER_API_H

#include <Arduino.h>

class DeveloperAPI
{
public:
    void begin();
    void update();

    String getName();
    String getRole();
    String getGitHub();
    String getLinkedIn();
    String getProject();

    // New functions
    int getRepos();
    int getStars();
    String getStatus();
};

extern DeveloperAPI developerAPI;

#endif