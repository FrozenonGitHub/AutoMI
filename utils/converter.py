#!/usr/bin/env python
"""
Convert single-instance GASVertexProgram to multi-instance version.

Usage: python converter.py [options] <input-filename> <output-filename>
"""

import sys
import os
import re
from optparse import OptionParser

from program_ast import *
from clang_helper import *

class GlobalConverter:
    all_segment_dict = {}
    vertex_data_obj = VertexData()
    edge_data_obj = EdgeData()

    def __init__(self, input_filename, output_filename, track_free):
        self.input_filename = input_filename
        self.output_filename = output_filename
        self.track_free = track_free
        self.msg_type_obj = MsgType(track_free)
        self.vertex_program_obj = VertexProgram(track_free)
        self.all_segment_dict['Patch'] = ['#include <limits>']

    def read_input_file(self):
        with open(self.input_filename, 'r') as file:
            content_str = file.read()
            self.content_list = content_str.split('\n')
            # locate user provided typedef and variables starting with "#pragma AUTOMI"
            for line_num in range(len(self.content_list)):
                content_line = self.content_list[line_num]
                if (content_line.startswith("#pragma AUTOMI")):
                    segment_start = line_num + 1
                    while True:
                        line_num += 1
                        if self.content_list[line_num].strip() == "":
                            break
                    segment_end = line_num
                    # debug print
                    print(f'segment_start: {segment_start}, segment_end: {segment_end}')
                    self.all_segment_dict['Patch'].extend(self.content_list[segment_start:segment_end])
                    break
            for (segment_start, segment_end) in locate_struct_segments(content_str):
                # debug print
                print(f'segment_start: {segment_start}, segment_end: {segment_end}')
                self.store_code_segment(self.content_list[segment_start-1:segment_end])


    def store_code_segment(self, code_segment : List[str]) -> None:
        if code_segment[0].strip().startswith('struct'):
            struct_def_list = code_segment[0].strip().split(' ')
            if 'vertex_data' in struct_def_list[1]:\
                self.all_segment_dict['VertexData'] = code_segment
            elif 'edge_data' in struct_def_list[1]:
                self.all_segment_dict['EdgeData'] = code_segment
            elif 'msg_type' in struct_def_list[1]:
                self.all_segment_dict['MsgType'] = code_segment
        else:
            assert(code_segment[0].strip().startswith('class'))
            self.all_segment_dict['VertexProgram'] = code_segment

    def annotate_all_segments(self) -> None:
        if 'VertexData' in self.all_segment_dict:
            self.annotate_vertex_data()
        if 'EdgeData' in self.all_segment_dict:
            self.annotate_edge_data()
        if 'MsgType' in self.all_segment_dict:
            self.annotate_msg_type()
        if 'VertexProgram' in self.all_segment_dict:
            self.annotate_vertex_program()

    def annotate_vertex_data(self) -> None:
        # debug print
        print('Parsing VertexData...')
        struct_lines = self.all_segment_dict['VertexData']
        code_lines = '\n'.join(self.all_segment_dict['Patch']) + '\n' + '\n'.join(struct_lines)
        self.vertex_data_obj.annotate(code_lines)
    
    def annotate_edge_data(self) -> None:
        struct_lines = self.all_segment_dict['EdgeData']
        code_lines = '\n'.join(self.all_segment_dict['Patch']) + '\n' + '\n'.join(struct_lines)
        self.edge_data_obj.annotate(code_lines)

    def annotate_msg_type(self) -> None:
        # debug print
        print('Annotating MsgType...')
        struct_lines = self.all_segment_dict['MsgType']
        code_lines = '\n'.join(self.all_segment_dict['Patch']) + '\n' + '\n'.join(struct_lines)
        self.msg_type_obj.annotate(code_lines)
    
    def annotate_vertex_program(self) -> None:
        # debug print
        print('Annotating VertexProgram...')
        graph_type_line = 'typedef graphlab::distributed_graph<vertex_data, edge_data> graph_type;\n'
        func_get_other_vertex_lines = ['inline graph_type::vertex_type\n',
                                       'get_other_vertex(const graph_type::edge_type& edge,\n',
                                       '                 const graph_type::vertex_type& vertex) {\n',
                                       '  return edge.source().id() == vertex.id() ? edge.target() : edge.source();\n',
                                       '}\n']
        self.helper_lines = graph_type_line + ''.join(func_get_other_vertex_lines)
        code_lines = '\n'.join(self.all_segment_dict['Patch']) + '\n' + self.helper_lines + self.vertex_program_keyword_replacement()
        self.vertex_program_obj.annotate(code_lines)

    def codegen(self):
        with open(self.output_filename, 'w') as file:
            file.write('#include <graphlab.hpp>\n')
            file.write('\n'.join(self.all_segment_dict['Patch']))
            file.write('\n')
            self.vertex_data_obj.codegen(file)
            file.write('\n')
            self.edge_data_obj.codegen(file)
            file.write('\n')
            self.msg_type_obj.codegen(file)
            file.write('\n')
            file.write(self.helper_lines)
            file.write('\n')
            self.vertex_program_obj.codegen(file)

    def vertex_program_keyword_replacement(self) -> str:
        field_keyword_list = []
        field_keyword_list.extend(['vertex_data_' + field[0] for field in self.vertex_data_obj.var_tuple_list])
        field_keyword_list.extend(['edge_data_' + field[0] for field in self.edge_data_obj.var_tuple_list])
        field_keyword_list.extend(['other_data_' + field[0] for field in self.vertex_data_obj.var_tuple_list])
        code_lines = 'double %s;\n' % ', '.join(field_keyword_list)
        code_lines += '\n'.join(self.content_list)
        code_lines = code_lines.replace('vertex.data().', 'vertex_data_')
        code_lines = code_lines.replace('edge.data().', 'edge_data_')
        code_lines = code_lines.replace('other.data().', 'other_data_')
        # Signal function call keyword replacement
        pattern = r"context\.signal\((.*?), (.*?)\);"
        unique_signal_lines = set(re.findall(pattern, code_lines))
        signal_keyword_list = [f"void context_signal_{param1}_{param_2}() {{}}\n" for param1, param_2 in unique_signal_lines]
        code_lines = re.sub(pattern, r"context_signal_\1_\2();", code_lines)
        code_lines = ''.join(signal_keyword_list) + code_lines
        return code_lines

# Configure and parse command-line arguments
def parse_args():
    parser = OptionParser(usage="python converter.py [options] <input-filename> <output-filename>", 
                          add_help_option=False)
    parser.add_option("-t", "--track-free", action="store_true", dest="track_free",
                      default=False, help="Enable TrackFree")
    (opts, args) = parser.parse_args()
    if len(args) != 2:
        parser.print_help()
        sys.exit(1)
    return opts, args

def main():
    opts, args = parse_args()
    input_filename, output_filename = args[0], args[1]
    # # debug: check opts is working
    # print("TrackFree enabled: ", opts.track_free)
    converter = GlobalConverter(input_filename, output_filename, opts.track_free)
    converter.read_input_file()
    converter.annotate_all_segments()
    converter.codegen()

if __name__ == "__main__":
    main()