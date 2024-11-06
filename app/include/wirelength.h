#pragma once

int reportWirelength();

int getRelatedWirelength(bool isBaseline, const std::set<int>& instRelatedNetId, std::string wlType = "default");

int getWirelength(bool isBaseline, std::string wlType = "default");

int getHPWL(bool isBaseline);