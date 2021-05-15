# Contribution Guide

## Overview

1. Please fork this repository
2. Commit your local repository and add Signed-Off information
3. Please send pull request to develop

## Issue

- Please check that you report bug for corresponding repository
- Please check that if is already reported
- Please use issue templete whenever possible
  - Please fill issue description
  - Please select Label
- If you're working on issue, please set assinees to assign yourself

## Pull Request

- Please select target branch to "develop" branch.
- Please use Pull Request templete whenever possible
  - Please fill issue description
  - Please confirm check list
  - Please select Label
- Please set reviwers to maintainer
- Please set assinees to assign reviewers

## Develop new Tools

- If you want to add feature tools, Please copy templete directory
- Please update Makefile.am for new tool
- Please add test scripts for new tool

## Commit Message

```
[${commit_type}] ${feature}: ${outline}

${Detail}
```

- 1st line - commit outline
  - Select the appropriate `commit_type` from the following
    - Add: add new feature
    - Fix: fix current bug
    - Improve: update other
  - describe `feature` name (e.g. check, stat,...)
- 2nd line - Blank
- 3rd line - Detail

## Branch rules

- main: release branch. [Protected]
- develop: developing branch. Will merge into master.
- bugfix: To be fixed when bugs are discovered.

## Coding Style

- Please check https://github.com/LeavaTail/debugfatfs/wiki/CodingStyle
