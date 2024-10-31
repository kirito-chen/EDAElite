#pragma once

int reportWirelength();

int getRelatedWirelength(bool isBaseline, const std::set<int>& instRelatedNetId);

int getWirelength(bool isBaseline);

int getHPWL(bool isBaseline);