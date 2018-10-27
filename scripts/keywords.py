#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2018 Onchere Bironga
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
import os, re
from utils import read, write

INTERNAL_DATA_TYPES = [
	"bool", "int", "uint", "int64", "uint64", "char",
	"void", "half", "float", "double", "auto"
]

INTERNAL_CONSTANTS = [
	"nullptr", "this", "main", "__ctor", "__dtor"
]

INTERNAL_TAGS = [
	"inline", "mustinline"
]

def getKeywordsList():
	lines = read("../" + os.path.dirname(__file__) + "whack.grammar")
	keywords = []
	for line in lines.split('\n'):
		match = re.findall('"[a-zA-Z_]+"', line)
		if match != None:
			for keyword in match:
				if keyword not in keywords:
					keywords.append(keyword)
	for keyword in INTERNAL_DATA_TYPES:
		keywords.append('"' + keyword + '"')
	for keyword in INTERNAL_CONSTANTS:
		keywords.append('"' + keyword + '"')
	for keyword in INTERNAL_TAGS:
		keywords.append('"' + keyword + '"')
	return keywords

def genKeywordsList():
	keywords = getKeywordsList()
	s = ''
	for keyword in keywords:
		s += keyword[1:-1] + '|'
	write('../keywords.txt', s[:-1] + "\n\n")
	reserved = "inline constexpr static auto RESERVED = {" + ', '.join(keywords) + "};"
	write('../reserved.def', reserved)

def main():
    genKeywordsList()

if __name__ == "__main__":
    main()
