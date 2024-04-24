#!/usr/bin/env python
"""
Data structure for program Abstract Syntax Tree (AST).
"""

from clang_helper import *
from typing import List
import pdb

# struct `vertex_data` in GAS program
class VertexData:
    struct_name = 'vertex_data'
    # record each member variable as a tuple:
    #   (var_name, var_type, default_value)
    var_tuple_list = []

    def __init__(self, struct_name='vertex_data') -> None:
        self.struct_name = struct_name

    def annotate(self, code_lines : str) -> None:
        constructor_params = parse_constructor_parameters(code_lines, 'vertex_data', 'vertex_data')
        self.var_tuple_list = constructor_params

    def codegen(self, output_file) -> None:
        output_file.write(''.join(self.multi_lines()))

    def multi_lines(self) -> List[str]:
        mip_lines = []

        # struct declare line
        mip_lines.append('struct vertex_data {\n')

        # struct fields declaration lines
        for (field_name, field_type, _) in self.var_tuple_list:
            mip_lines.append('  graphlab::automi_bitvec<%s> %s;\n' % (field_type, field_name))

        # struct default constructor
        mip_lines.append('  vertex_data() {\n')
        for (field_name, field_type, field_value) in self.var_tuple_list:
            mip_lines.append('    %s = graphlab::automi_bitvec<%s>(NUM_SRC_NODES);\n' % (field_name, field_type))
            mip_lines.append('    %s.set_all(%s);\n' % (field_name, field_value))
        mip_lines.append('  }\n')

        # struct explicit constructor
        constructor_params_list = ['const graphlab::automi_bitvec<%s>& %s' % (var_tuple[1], var_tuple[0]) for var_tuple in self.var_tuple_list]
        constructor_params_str = ', '.join(constructor_params_list)
        constructor_field_list = ['%s(%s)' % (var_tuple[0], var_tuple[0]) for var_tuple in self.var_tuple_list]
        constructor_field_str = ', '.join(constructor_field_list)
        mip_lines.append('  explicit vertex_data(%s) : %s {}\n' % (constructor_params_str, constructor_field_str))

        # `save` method for serialization
        mip_lines.append('  void save(graphlab::oarchive &oarc) const {\n')
        for (var_name, _, _) in self.var_tuple_list:
            mip_lines.append('    oarc << %s;\n' % var_name)
        mip_lines.append('  }\n')

        # `load` method for serialization
        mip_lines.append('  void load(graphlab::iarchive &iarc) {\n')
        for (var_name, _, _) in self.var_tuple_list:
            mip_lines.append('    iarc >> %s;\n' % var_name)
        mip_lines.append('  }\n')
        # end of struct line
        mip_lines.append('}; // end of vertex_data\n')
        return mip_lines

# struct `edge_data` in GAS program
class EdgeData:
    struct_name = 'edge_data'
    is_empty = True
    is_pod = False   # `IS_POD_TYPE`
    # record each member variable as a tuple:
    #   (var_name, var_type, default_value)
    var_tuple_list = []

    def __init__(self, struct_name='edge_data') -> None:
        self.struct_name = struct_name

    def annotate(self, code_lines : str) -> None:
        if 'IS_POD_TYPE' in code_lines:
            self.is_pod = True
        constructor_params = parse_constructor_parameters(code_lines, 'edge_data', 'edge_data')
        self.var_tuple_list = constructor_params
        self.is_empty = False

    def codegen(self, output_file) -> None:
        if self.is_empty:
            output_file.write('typedef graphlab::empty edge_data;\n')
        else:
            output_file.write(''.join(self.multi_lines()))
        

    def multi_lines(self) -> List[str]:
        mip_lines = []
        
        # struct declare line
        if self.is_pod:
            mip_lines.append('struct %s : graphlab::IS_POD_TYPE {\n' % self.struct_name)
        else:
            mip_lines.append('struct %s {\n' % self.struct_name)
        
        # member variables
        for (var_name, var_type, _) in self.var_tuple_list:
            mip_lines.append('  %s %s;\n' % (var_type, var_name))
        
        # constructor line
        params_list = [f'{var_type} {var_name} = {default_value}' for (var_name, var_type, default_value) in self.var_tuple_list]
        params_str = ', '.join(params_list)
        fields_list = [f'{var_name}({var_name})' for (var_name, _, _) in self.var_tuple_list]
        fields_str = ', '.join(fields_list)
        mip_lines.append('  %s(%s) : %s {}\n' % (self.struct_name, params_str, fields_str))

        # serialization if not `IS_POD_TYPE`
        if not self.is_pod:
            # `save` method for serialization
            mip_lines.append('  void save(graphlab::oarchive &oarc) const {\n')
            for (var_name, _, _) in self.var_tuple_list:
                mip_lines.append('    oarc << %s;\n' % var_name)
            mip_lines.append('  }\n')
            # `load` method for serialization
            mip_lines.append('  void load(graphlab::iarchive& iarc) {\n')
            for (var_name, _, _) in self.var_tuple_list:
                mip_lines.append('    iarc >> %s;\n' % var_name)
            mip_lines.append('  }\n')
        
        # end of struct edge_data
        mip_lines.append('}; // end of edge_data\n')

        return mip_lines


# struct `msg_type` in GAS program
class MsgType:
    struct_name = 'msg_type'
    track_free = False
    var_tuple_list = []
    accum_op_lines_list = []

    def __init__(self, track_free) -> None:
        self.track_free = track_free

    def annotate(self, code_lines : str) -> None:
        constructor_params = parse_constructor_parameters(code_lines, 'msg_type', 'msg_type')
        self.var_tuple_list = constructor_params
        # TODO: handle operator+= function!
        self.accum_op_lines_list.extend(parse_accum_operator(code_lines, 'msg_type'))

    def codegen(self, output_file) -> None:
        output_file.write(''.join(self.multi_lines()))

    def multi_lines(self) -> List[str]:
        mip_lines = []

        # struct declare line
        mip_lines.append('struct %s {\n' % self.struct_name)

        # struct field declaration lines
        for (field_name, field_type, _) in self.var_tuple_list:
            mip_lines.append('  graphlab::automi_bitvec<%s> %s;\n' % (field_type, field_name))
        if not self.track_free:
            mip_lines.append('  graphlab::automi_bitvec<bool> track;\n')

        # default constructor
        mip_lines.append('  %s() {\n' % self.struct_name)
        for (field_name, field_type, field_value) in self.var_tuple_list:
            mip_lines.append('    %s = graphlab::automi_bitvec<%s>(NUM_SRC_NODES);\n' % (field_name, field_type))
            mip_lines.append('    %s.set_all(%s);\n' % (field_name, field_value))
        if not self.track_free:
            mip_lines.append('    track = graphlab::automi_bitvec<bool>(NUM_SRC_NODES);\n')
            mip_lines.append('    track.set_all(false);\n')
        mip_lines.append('  }\n')

        # source-specific constructor
        source_default_values_list = []
        for (field_name, field_type, _) in self.var_tuple_list:
            source_default_values_list.append('%s %s' % (field_type, field_name + '_in'))
        mip_lines.append('  %s(%s, size_t idx) {\n' % (self.struct_name, ', '.join(source_default_values_list)))
        for (field_name, field_type, field_value) in self.var_tuple_list:
            mip_lines.append('    %s = graphlab::dimitra_bitvec<%s>(NUM_SRC_NODES);\n' % (field_name, field_type))
            mip_lines.append('    %s.set_all(%s);\n' % (field_name, field_value))
            mip_lines.append('    %s.set_single(%s, idx);\n' % (field_name, field_name + '_in'))
        if not self.track_free:
            mip_lines.append('    track = graphlab::dimitra_bitvec<bool>(NUM_SRC_NODES);\n')
            mip_lines.append('    track.set_all(false);\n')
            mip_lines.append('    track.set_single(true, idx);\n')
        mip_lines.append('  }\n')

        # explicit constructor
        explicit_constructor_params_list = []
        for (field_name, field_type, _) in self.var_tuple_list:
            explicit_constructor_params_list.append('const graphlab::automi_bitvec<%s>& %s' % (field_type, field_name + '_in'))
        if not self.track_free:
            explicit_constructor_params_list.append('const graphlab::automi_bitvec<bool>& track_in')
        mip_lines.append('  %s(%s) {\n' % (self.struct_name, ', '.join(explicit_constructor_params_list)))
        for (field_name, field_type, _) in self.var_tuple_list:
            mip_lines.append('    %s = graphlab::automi_bitvec<%s>(%s);\n' % (field_name, field_type, field_name + '_in'))
        if not self.track_free:
            mip_lines.append('    track = graphlab::automi_bitvec<bool>(track_in);\n')
        mip_lines.append('  }\n')

        # accumulation funtion `operator+=`
        mip_lines.append('  %s& operator+=(const %s& other) {\n' % (self.struct_name, self.struct_name))
        if not self.track_free:
            mip_lines.append('    graphlab::automi_bitvec::pair_op_or(track, other.track);\n')
        for accum_line in self.accum_op_lines_list:
            if len(accum_line.split('=')) != 2:
                break
            [tmp_field_name, operation_str] = accum_line.split('=')
            _, operator_name = pair_op_lookup(operation_str)
            for (field_name, field_type, _) in self.var_tuple_list:
                if field_name == tmp_field_name:
                    tmp_field_type = field_type
                    mip_lines.append('    graphlab::automi_bitvec<%s>::%s(%s, other.%s);\n' % (tmp_field_type, operator_name, tmp_field_name, tmp_field_name))
                    break
            mip_lines.append('    return *this;\n')
            mip_lines.append('  }\n')

        # `save` method for serialization
        mip_lines.append('  void save(graphlab::oarchive &oarc) const {\n')
        for (field_name, _, _) in self.var_tuple_list:
            mip_lines.append('    oarc << %s;\n' % field_name)
        if not self.track_free:
            mip_lines.append('    oarc << track;\n')    # save `track`
        mip_lines.append('  }\n')

        # `load` method for serialization
        mip_lines.append('  void load(graphlab::iarchive& iarc) {\n')
        for (field_name, _, _) in self.var_tuple_list:
            mip_lines.append('    iarc >> %s;\n' % field_name)
        if not self.track_free:
            mip_lines.append('    iarc >> track;\n')    # load `track`
        mip_lines.append('  }\n')

        # end of struct msg_type
        mip_lines.append('}; // end of msg_type\n')

        return mip_lines

# class `vertex_program` in GAS program
class VertexProgram:
    class_name = 'vertex_program'
    track_free = False
    vp_fields = []

    def __init__(self, track_free) -> None:
        self.track_free = track_free

    def annotate(self, code_lines : str) -> None:
        code_clang_index = clang.cindex.Index.create()
        tu = code_clang_index.parse('temp.cpp', args=['-std=c++11'], unsaved_files=[('temp.cpp', code_lines)])

        # parse fields
        self.vp_fields = parse_struct_fields(code_lines, 'vertex_program')

        # gather_edges function
        self.gather_edges_lines = locate_method_extent(code_lines, 'vertex_program', 'gather_edges')

        # scatter_edges function
        self.scatter_edges_lines = locate_method_extent(code_lines, 'vertex_program', 'scatter_edges')

        # TODO: (1) store function header code lines
        #       (2) label function body statements

        # gather function
        self.gather_params, self.gather_body = parse_cxx_method(tu, 'gather')

        # apply function
        self.apply_params, self.apply_body = parse_cxx_method(tu, 'apply')

        # scatter function
        self.scatter_params, self.scatter_body = parse_cxx_method(tu, 'scatter')



    def codegen(self, output_file) -> None:
        replacement_dict = {
            'vertex_data_': 'vertex.data().',
            'edge_data_': 'edge.data().',
            'other_data_': 'other.data().'
        }
        code_lines = ''.join(self.multi_lines())
        for (old_str, new_str) in replacement_dict.items():
            code_lines = code_lines.replace(old_str, new_str)
        output_file.write(code_lines)

    def multi_lines(self) -> List[str]:
        mip_lines = []

        # class declare line
        mip_lines.append('class vertex_program : public graphlab::ivertex_program<graph_type, msg_type, msg_type> {\n')

        # declare member variables
        for (field_name, field_type) in self.vp_fields:
            mip_lines.append('  graphlab::automi_bitvec<%s> %s;\n' % (field_type, field_name))
        if not self.track_free:
            mip_lines.append('  graphlab::automi_bitvec<bool> vp_track;\n')
        
        mip_lines.append('public:\n')

        # gather_edges function, kept the same as single-instance
        mip_lines.append('\n  ')
        mip_lines.append('  \n'.join(self.gather_edges_lines))

        # gather function
        gather_params_line = ', '.join([f'{param_type} {param_name}' for (param_name, param_type) in self.gather_params])
        mip_lines.append('\n  ' + 'msg_type gather(%s) const {' % gather_params_line)
        mip_lines.append('\n  ' + '\n  '.join(convert_comp_stmt(self.gather_body, True)))
        mip_lines.append('\n  }\n')

        # apply function
        apply_params_line = ', '.join([f'{param_type} {param_name}' for (param_name, param_type) in self.apply_params])
        mip_lines.append('\n  ' + 'void apply(%s) {\n' % apply_params_line)
        if not self.track_free:
            mip_lines.append('    vp_track = msg_acc.track;\n')
        mip_lines.append('\n  ' + '\n  '.join(convert_comp_stmt(self.apply_body, self.track_free)))
        mip_lines.append('\n  }\n')

        # scatter function
        scatter_params_line = ', '.join([f'{param_type} {param_name}' for (param_name, param_type) in self.scatter_params])
        mip_lines.append('\n  ' + 'void scatter(%s) const {' % scatter_params_line)
        scatter_body_lines = '\n  '.join(convert_comp_stmt(self.scatter_body, self.track_free))
        scatter_body_lines = scatter_body_lines.replace('msg_acc.track', 'vp_track')
        mip_lines.append('\n  ' + scatter_body_lines)
        mip_lines.append('\n  }\n')

        # scatter_edges function, kept the same as single-instance
        mip_lines.append('\n  ')
        mip_lines.append('  \n'.join(self.scatter_edges_lines) + '\n')

        # `save` method for serialization
        mip_lines.append('\n  void save(graphlab::oarchive &oarc) const {\n')
        for (field_name, _) in self.vp_fields:
            mip_lines.append('    oarc << %s;\n' % field_name)
        if not self.track_free:
            mip_lines.append('    oarc << vp_track;\n')
        mip_lines.append('  }\n')

        # `load` method for serialization
        mip_lines.append('\n  void load(graphlab::iarchive& iarc) {\n')
        for (field_name, _) in self.vp_fields:
            mip_lines.append('    iarc >> %s;\n' % field_name)
        if not self.track_free:
            mip_lines.append('    iarc >> vp_track;\n')
        mip_lines.append('  }\n')

        # end of vertex program class
        mip_lines.append('\n')
        mip_lines.append('}; // end of vertex program class\n')

        return mip_lines
