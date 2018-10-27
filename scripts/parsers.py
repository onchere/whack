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

def getGrammarList():
    lines = read("../whack.grammar")
    grammars = []
    for line in lines.split('\n'):
        match = re.match('[a-zA-Z]+', line)
        if match != None:
            grammars.append(match.group())
    return grammars

def genParserList():
    l, out, num, num2 = '', '', 0, 0
    includes = ''
    for grammar in getGrammarList():
        l += grammar + ', '
        out += 'parser(' + grammar + '); '
    write('../parserlist.def', '#define parsers ' + l[:-2] + '\n')
    write('../parsermembers.def',
                '#define parser(p) mpc_parser_t* p{mpc_new(#p)}\n' +
                out[:-1] + 
                '\n#undef parser')

def main():
    genParserList()

if __name__ == "__main__":
    main()
