"""
    Helper functions for code parsing
"""

import clang.cindex
import re
import pdb

from typing import List
from anytree import Node, RenderTree

mask_count = 0

pair_op_dict = {'std::min': 'pair_op_min', 'std::max': 'pair_op_max',
                '|': 'pair_op_or', '&': 'pair_op_and',
                '+': 'pair_op_plus', '*': 'pair_op_mul'}

def pair_op_lookup(op_str):
    for key, value in pair_op_dict.items():
        if key in op_str:
            return key, value
    return None, None

vec_op_dict = {'+': 'vec_op_add_update_mask', '-': 'vec_op_sub_update_mask',
               '*': 'vec_op_mul_update_mask', '/': 'vec_op_div_update_mask',
               '==': 'vec_op_cmpeq_update_mask', '!=': 'vec_op_cmpneq_update_mask',
               '<=': 'vec_op_cmple_update_mask', '>=': 'vec_op_cmpge_update_mask',
               '<': 'vec_op_cmplt_update_mask', '>': 'vec_op_cmpgt_update_mask',
               '&~': 'vec_op_andnot_update_mask', '|': 'vec_op_or_update_mask', '&': 'vec_op_and_update_mask',
               '~': 'vec_op_neg_update_mask'}

def vec_op_lookup(op_str):
    for key, value in vec_op_dict.items():
        if key in op_str:
            return key, value
    return None, None

def first_last_paren(input : str, paren='()'):
    if paren == '()':
        first_open_paren = re.search(r'\(', input)
        last_close_paren = re.search(r'\)[^)]*$', input)
        if first_open_paren and last_close_paren:
            header = input[:first_open_paren.start()]
            content = input[first_open_paren.start() : last_close_paren.end()]
            return [header.strip(), content.strip()]

def extract_default_argument_value(argument):
    # Check if the argument has a default value
    tokens = list(argument.get_tokens())
    for i in range(len(tokens)):
        if tokens[i].spelling == '=':
            default_value_tokens = tokens[i+1:]
            default_value = ''.join([t.spelling for t in default_value_tokens])
            return default_value.strip()

    return None

def parse_constructor_parameters(cpp_code, struct_name, constructor_name):
    index = clang.cindex.Index.create()
    tu = index.parse('temp.cpp', args=['-std=c++17'], unsaved_files=[('temp.cpp', cpp_code)])

    for node in tu.cursor.walk_preorder():
        if (
            node.kind == clang.cindex.CursorKind.CONSTRUCTOR
            and node.spelling == constructor_name
            and node.semantic_parent.spelling == struct_name
        ):
            parameters = []

            for param in node.get_arguments():
                param_type = param.type.spelling
                param_name = param.spelling

                default_value = extract_default_argument_value(param)
                if default_value is not None:
                    parameter_tuple = (param_name, param_type, default_value)
                else:
                    parameter_tuple = (param_name, param_type, None)
                parameters.append(parameter_tuple)

            return parameters

    return None

def parse_struct_fields(cpp_code, struct_name):
    index = clang.cindex.Index.create()
    tu = index.parse('temp.cpp', args=['-std=c++17'], unsaved_files=[('temp.cpp', cpp_code)])

    fields = []

    for node in tu.cursor.walk_preorder():
        if (node.kind == clang.cindex.CursorKind.FIELD_DECL
            and node.semantic_parent.spelling == struct_name):
            default_value = extract_default_argument_value(node)
            if default_value:
                fields.append((node.spelling, node.type.spelling, default_value))
            else:
                fields.append((node.spelling, node.type.spelling))
    
    if len(fields) > 0:
        return fields
    else:
        return None
    
def parse_accum_operator(cpp_code, struct_name):
    def get_binary_operation(node):
        if node.kind == clang.cindex.CursorKind.BINARY_OPERATOR:
            return node.spelling

        for child in node.get_children():
            op = get_binary_operation(child)
            if op:
                return op
    
    index = clang.cindex.Index.create()
    tu = index.parse('temp.cpp', args=['-std=c++17'], unsaved_files=[('temp.cpp', cpp_code)])

    operation_lines_list = []

    for node in tu.cursor.walk_preorder():
        if (node.kind == clang.cindex.CursorKind.CXX_METHOD and 
            node.spelling == 'operator+=' and
            node.semantic_parent.spelling == struct_name):
                start_line = node.extent.start.line + 1
                end_line = node.extent.end.line
                tokens = tu.get_tokens(extent=node.extent)
                for i in range(start_line, end_line):
                    line_tokens = [token.spelling for token in tokens if token.location.line == i]
                    operation_lines_list.append(''.join(line_tokens))
    
    return operation_lines_list


def parse_expr_tokens(token_list) -> Node:
    expr_root = Node('expr_root', type='expr_root')
    expr_str = ''.join(token_list)
    op_key, op_value = vec_op_lookup(expr_str)
    if op_key:
        expr_split_list = expr_str.split(op_key)
        if len(expr_split_list) == 2:
            expr_root.name = op_key
            expr_root.type = 'expr_bi_op'
            expr_root.children = [Node(expr_split_list[0], type='expr_operand'), Node(expr_split_list[1], type='expr_operand')]
        else:
            expr_root.name = op_key
            expr_root.type = 'expr_un_op'
            expr_root.children = [Node(expr_split_list[0], type='expr_operand')]
    else:
        expr_root.name = expr_str
        expr_root.type = 'expr_no_op'
    return expr_root


def locate_method_extent(cpp_code, class_name, method_name):
    index = clang.cindex.Index.create()
    tu = index.parse('temp.cpp', args=['-std=c++17'], unsaved_files=[('temp.cpp', cpp_code)])

    for node in tu.cursor.walk_preorder():
        if (node.kind == clang.cindex.CursorKind.CXX_METHOD and node.spelling == method_name and node.semantic_parent.spelling == class_name):
            code = []
            line = ""
            prev_token = None
            for tok in node.get_tokens():
                if prev_token is None:
                    prev_token = tok
                prev_location = prev_token.location
                prev_token_end_col = prev_location.column + len(prev_token.spelling)
                cur_location = tok.location
                if cur_location.line > prev_location.line:
                    code.append(line)
                    line = " " * (cur_location.column - 1)
                else:
                    if cur_location.column > (prev_token_end_col):
                        line += " "
                line += tok.spelling
                prev_token = tok
            if len(line.strip()) > 0:
                code.append(line)
            return code
    
    return None


def locate_struct_segments(cpp_code):
    index = clang.cindex.Index.create()
    tu = index.parse('temp.cpp', args=['-std=c++11'], unsaved_files=[('temp.cpp', cpp_code)])

    struct_name_list = ['vertex_data', 'edge_data', 'msg_type']

    segment_start_end_tuples = []

    for node in tu.cursor.walk_preorder():
        if (
            node.kind == clang.cindex.CursorKind.CLASS_DECL and 
            node.semantic_parent.spelling == 'temp.cpp'
        ):
            segment_start_end_tuples.append((node.extent.start.line, node.extent.end.line))
        elif (
            node.kind == clang.cindex.CursorKind.STRUCT_DECL and 
            node.spelling in struct_name_list and 
            node.semantic_parent.spelling == 'temp.cpp'
        ): 
            segment_start_end_tuples.append((node.extent.start.line, node.extent.end.line))

    return segment_start_end_tuples


def parse_cxx_method(tu, method_name):
    for cursor in tu.cursor.walk_preorder():
        if (cursor.kind == clang.cindex.CursorKind.CXX_METHOD and cursor.spelling == method_name):
            # debug print
            print('==========')
            print('CXX_METHOD method_name cursor.spelling: %s' % (cursor.spelling))
            print('==========')
            param_tuple_list = []
            global mask_count
            mask_count = 0 # reset mask_count
            for direct_child in cursor.get_children():
                if direct_child.kind == clang.cindex.CursorKind.PARM_DECL:
                    param_tuple_list.append(parse_param_decl(direct_child))
                elif direct_child.kind == clang.cindex.CursorKind.COMPOUND_STMT:
                    body_root = annotate_compound_stmt(direct_child, 'T')
            return param_tuple_list, body_root


def parse_param_decl(cursor) -> tuple:
    assert cursor.kind == clang.cindex.CursorKind.PARM_DECL
    param_tokens = [token.spelling for token in cursor.get_tokens()]
    param_name = param_tokens[-1]
    param_type = ' '.join(param_tokens[0:-1])
    # raw_type = param_type.strip('const').strip('&').strip()
    return (param_name, param_type)


def annotate_compound_stmt(cursor, automi_label) -> Node:
    assert cursor.kind == clang.cindex.CursorKind.COMPOUND_STMT
    stmt_root_list = []

    comp_stmt_root = Node('CompoundStmt', type='comp_stmt', mark=automi_label)

    for child_stmt in cursor.get_children():
        # # debug print
        # print('child_stmt.kind: %s' % (child_stmt.kind))
        if child_stmt.kind == clang.cindex.CursorKind.IF_STMT:
            if_stmt_root = annotate_if_stmt(child_stmt, automi_label)
            stmt_root_list.append(if_stmt_root)
        elif child_stmt.kind == clang.cindex.CursorKind.BINARY_OPERATOR:
            assign_stmt_root = annotate_assign_stmt(child_stmt, automi_label)
            stmt_root_list.append(assign_stmt_root)
        elif child_stmt.kind == clang.cindex.CursorKind.DECL_STMT:
            decl_stmt_root = annotate_decl_stmt(child_stmt, automi_label)
            stmt_root_list.append(decl_stmt_root)
        elif child_stmt.kind == clang.cindex.CursorKind.CALL_EXPR:
            signal_root = annotate_signal(child_stmt, automi_label)
            stmt_root_list.append(signal_root)
        elif child_stmt.kind == clang.cindex.CursorKind.RETURN_STMT:
            return_stmt_root = annotate_return_stmt(child_stmt, automi_label)
            stmt_root_list.append(return_stmt_root)
        # else:
        #     raise ValueError('Unknown statement type')

    for stmt_root in stmt_root_list:
        stmt_root.parent = comp_stmt_root
        # # debug print
        # print(RenderTree(stmt_root))

    return comp_stmt_root


def annotate_if_stmt(cursor, automi_label) -> Node:
    assert cursor.kind == clang.cindex.CursorKind.IF_STMT
    # FORMAT:
    # if (<condition-expr>) {
    #     <compound stmt>
    # } else {
    #     <compound stmt>
    # }
    if_stmt_root = Node('IF_ROOT', type='if_stmt', mark=automi_label)
    else_branch = False
    global mask_count
    mask_count += 1 # increment mask_count
    stmt_t_mark = 'mask_' + str(mask_count)
    for if_child_stmt in cursor.get_children():
        if (if_child_stmt.kind == clang.cindex.CursorKind.BINARY_OPERATOR) or (if_child_stmt.kind == clang.cindex.CursorKind.UNEXPOSED_EXPR):
            if_cond_root = annotate_if_condition(if_child_stmt, automi_label)
            if_cond_root.name = stmt_t_mark
            if_cond_root.parent = if_stmt_root
        elif (if_child_stmt.kind == clang.cindex.CursorKind.COMPOUND_STMT) and (not else_branch):
            if_branch_root = annotate_compound_stmt(if_child_stmt, stmt_t_mark)
            if_branch_root.parent = if_stmt_root
            else_branch = True
        elif (if_child_stmt.kind == clang.cindex.CursorKind.COMPOUND_STMT) and else_branch:
                mask_count = mask_count + 1 # increment mask_count
                stmt_f_mark = 'mask_' + str(mask_count)
                else_cond_root = Node(stmt_f_mark, type='else_cond', parent=if_stmt_root)
                else_cond_root.children = [Node('~', type='expr_un_op', parent=else_cond_root)]
                else_branch_root = annotate_compound_stmt(if_child_stmt, stmt_f_mark)
                else_branch_root.parent = if_stmt_root
    return if_stmt_root


def annotate_if_condition(cursor, automi_label) -> Node:
    cond_root = Node('IF_COND', type='if_cond', mark=automi_label)
    token_spelling_list = [token.spelling for token in cursor.get_tokens()]
    cond_str = ''.join(token_spelling_list)
    op_key, _ = vec_op_lookup(cond_str)
    if op_key:
        expr_split_list = cond_str.split(op_key)
        if len(expr_split_list) == 2:
            expr_root = Node(op_key, type='expr_bi_op', parent=cond_root)
            expr_root.children = [Node(expr_split_list[0], type='expr_operand'), Node(expr_split_list[1], type='expr_operand')]
        else:
            expr_root = Node(op_key, type='expr_un_op', parent=cond_root)
            expr_root.children = [Node(expr_split_list[0], type='expr_operand')]
    else:
        expr_root = Node(cond_str, type='expr_no_op', parent=cond_root)
    return cond_root


def annotate_assign_stmt(cursor, automi_label) -> Node:
    assert cursor.kind == clang.cindex.CursorKind.BINARY_OPERATOR
    # FORMAT:
    # <variable name> = <expression>;

    token_spelling_list = [token.spelling for token in cursor.get_tokens()]
    assert '=' in token_spelling_list
    eq_idx = token_spelling_list.index('=')

    lhs_var_name = ''.join(token_spelling_list[0:eq_idx])
    rhs_token_list = token_spelling_list[eq_idx+1:]

    assign_root_node = Node('=', type='assign_stmt', mark=automi_label)
    lhs_var_node = Node(lhs_var_name, type='lhs_var')
    lhs_var_node.parent = assign_root_node
    rhs_expr_node = parse_expr_tokens(rhs_token_list)
    rhs_expr_node.parent = assign_root_node

    return assign_root_node


def annotate_decl_stmt(cursor, automi_label) -> Node:
    assert cursor.kind == clang.cindex.CursorKind.DECL_STMT
    # FORMAT:
    # <typename> <variable_name> = <expression>;
    token_spelling_list = [token.spelling for token in cursor.get_tokens()]
    
    decl_root_node = Node('=', type='decl_stmt', mark=automi_label)
    decl_root_node.token_list = token_spelling_list
    return decl_root_node


def annotate_signal(cursor, automi_label) -> Node:
    assert cursor.kind == clang.cindex.CursorKind.CALL_EXPR
    # FORMAT:
    # context_signal_param1_param2();
    signal_root = Node('Signal', type='signal', mark=automi_label)
    signal_root.name = cursor.spelling
    return signal_root


def annotate_return_stmt(cursor, automi_label) -> Node:
    assert cursor.kind == clang.cindex.CursorKind.RETURN_STMT
    return_stmt_root = Node('ReturnStmt', type='return_stmt', mark=automi_label)
    for return_child in cursor.get_children():
        return_stmt_root.name = ' '.join([tok.spelling for tok in return_child.get_tokens()])
    return return_stmt_root


def convert_comp_stmt(comp_stmt_root : Node, ignore_track=False):
    child_stmt_code_list = []
    for child_stmt_root in comp_stmt_root.children:
        if child_stmt_root.type == 'if_stmt':
            child_stmt_code_list.extend(['  ' + line for line in convert_if_stmt(child_stmt_root, ignore_track)])
        elif child_stmt_root.type == 'signal':
            child_stmt_code_list.extend(['  ' + line for line in convert_call_signal(child_stmt_root, ignore_track)])
        elif child_stmt_root.type == 'assign_stmt':
            child_stmt_code_list.extend(['  ' + line for line in convert_assign_stmt(child_stmt_root, ignore_track)])
        elif child_stmt_root.type == 'decl_stmt':
            child_stmt_code_list.extend(['  ' + line for line in convert_decl_stmt(child_stmt_root, ignore_track)])
        elif child_stmt_root.type == 'return_stmt':
            child_stmt_code_list.extend(['  ' + line for line in convert_return_stmt(child_stmt_root, ignore_track)])
    return child_stmt_code_list


def convert_if_stmt(if_stmt_root : Node, ignore_track=False) -> List:
    if_stmt_code_list = []
    # if branch condition assignment
    if_cond_root = if_stmt_root.children[0]
    no_mask = (ignore_track) and (if_cond_root.mark == 'T')
    mask_name = 'msg_acc.track' if (if_cond_root.mark == 'T') else if_cond_root.mark
    if_stmt_code_list.append('graphlab::automi_bitvec<bool> %s;' % if_cond_root.name) # declare bitvec
    cond_expr_root = if_cond_root.children[0]
    if_cond_str = '%s.%s' % (if_cond_root.name, convert_cond_expr(cond_expr_root, '' if no_mask else mask_name))
    if_stmt_code_list.append(if_cond_str)
    # if branch stmt_t
    if_branch_root = if_stmt_root.children[1]
    if_stmt_code_list.extend(convert_comp_stmt(if_branch_root, ignore_track))
    # else branch
    if (len(if_stmt_root.children) == 4):
        else_cond_root = if_stmt_root.children[2]
        mask_name = 'msg_acc.track' if (if_cond_root.mark == 'T') else else_cond_root.mark
        if_stmt_code_list.append('graphlab::automi_bitvec<bool> %s;' % else_cond_root.name) # declare bitvec
        else_expr_root = else_cond_root.children[0]
        else_cond_str = '%s.%s' % (else_cond_root.name, convert_cond_expr(else_expr_root, '' if no_mask else mask_name))
        if_stmt_code_list.append(else_cond_str)
        else_branch_root = if_stmt_root.children[3]
        if_stmt_code_list.extend(convert_comp_stmt(else_branch_root, ignore_track))
    return if_stmt_code_list


def convert_cond_expr(cond_expr_root : Node, mask_name='') -> str:
    no_mask = (mask_name == '')
    vec_op_name = 'vec_op_set_mask' if cond_expr_root.type == 'expr_no_op' else vec_op_lookup(cond_expr_root.name)[1]
    vec_op_name = vec_op_name.strip('_mask') if no_mask else vec_op_name
    if cond_expr_root.type == 'expr_no_op':
        if no_mask:
            return f'{vec_op_name}({cond_expr_root.name});'
        else:
            return f'{vec_op_name}({mask_name}, {cond_expr_root.name});'
    elif cond_expr_root.type == 'expr_bi_op':
        if no_mask:
            return f'{vec_op_name}({cond_expr_root.children[0].name}, {cond_expr_root.children[1].name});'
        else:
            return f'{vec_op_name}({mask_name}, {cond_expr_root.children[0].name}, {cond_expr_root.children[1].name});'
    else:
        if no_mask:
            return f'{vec_op_name}({cond_expr_root.children[0].name});'
        else:
            return f'{vec_op_name}({mask_name}, {cond_expr_root.children[0].name});'


def convert_call_signal(signal_root : Node, ignore_track=False) -> List:
    signal_code_list = []
    mask_name = 'msg_acc.track' if (signal_root.mark == 'T') else signal_root.mark
    token_list = signal_root.name.split('_')
    vertex_str, msg_str = token_list[2], token_list[3]
    if ignore_track:
        if (signal_root.mark == 'T'):
            signal_code_list.append('context.signal(%s, %s);' % (vertex_str, msg_str))
        else:
            signal_code_list.append('if (!%s.vec_all_zeros()) {' % mask_name)
            signal_code_list.append('  context.signal(%s, %s);' % (vertex_str, msg_str))
            signal_code_list.append('}')
    else:
        signal_code_list.append('if (!%s.vec_all_zeros()) {' % mask_name)
        signal_code_list.append('  %s.track = %s;' % (msg_str, mask_name))
        signal_code_list.append('  context.signal(%s, %s);' % (vertex_str, msg_str))
        signal_code_list.append('}')
    return signal_code_list


def convert_assign_stmt(assign_stmt_root : Node, ignore_track=False) -> List:
    no_mask = (ignore_track) and (assign_stmt_root.mark == 'T')
    lhs_var_name = assign_stmt_root.children[0].name
    rhs_expr_root = assign_stmt_root.children[1]
    op_name = 'vec_op_set_mask' if rhs_expr_root.type == 'expr_no_op' else vec_op_lookup(rhs_expr_root.name)[1]
    vec_op_name = op_name.strip('_mask') if no_mask else op_name
    mask_name = 'msg_acc.track' if (assign_stmt_root.mark == 'T') else assign_stmt_root.mark
    # codegen based on operator type
    if rhs_expr_root.type == 'expr_no_op':
        if no_mask:
            return [f'{lhs_var_name}.{vec_op_name}({rhs_expr_root.name});']
        else:
            return [f'{lhs_var_name}.{vec_op_name}({mask_name}, {rhs_expr_root.name});']
    elif rhs_expr_root.type == 'expr_bi_op':
        if no_mask:
            return [f'{lhs_var_name}.{vec_op_name}({rhs_expr_root.children[0].name}, {rhs_expr_root.children[1].name});']
        else:
            return [f'{lhs_var_name}.{vec_op_name}({mask_name}, {rhs_expr_root.children[0].name}, {rhs_expr_root.children[1].name});']
    else:
        if no_mask:
            return [f'{lhs_var_name}.{vec_op_name}({rhs_expr_root.children[0].name});']
        else:
            return [f'{lhs_var_name}.{vec_op_name}({mask_name}, {rhs_expr_root.children[0].name});']


def convert_decl_stmt(decl_stmt_root : Node, ignore_track=False) -> List:
    return [' '.join(decl_stmt_root.token_list)]


def convert_return_stmt(return_stmt_root : Node, ignore_track=False) -> List:
    return ['return ' + return_stmt_root.name + ';']
