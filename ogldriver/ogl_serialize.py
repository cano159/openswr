# Copyright 2014 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import sys
import os
from argparse import ArgumentParser, RawTextHelpFormatter

CORRECT_IMPL="""template <typename... Ts> struct length;
template <typename T, typename... Ts>
struct length<T, Ts...>
{
        static const int value = 1 + length<Ts...>::value;
};
template <>
struct length<>
{
        static const int value = 0;
};

template <bool b, typename T> struct enable_if;
template <typename T> struct enable_if<true, T> { typedef T type; };

unsigned char* insertArguments(unsigned char* buffer)
{
        return buffer;
}

template <typename Arg, typename... Args>
unsigned char* insertArguments(unsigned char* buffer, Arg arg, Args... args)
{
        memcpy(buffer, reinterpret_cast<unsigned char*>(&arg), sizeof(Arg));
        buffer  += sizeof(Arg);
        return  insertArguments(buffer, args...);
}

template <typename Result, typename... Args>
unsigned char* insertCommand(int Cmd, unsigned char* buffer, Result (*fn)(Args...), Args... args)
{
        ((unsigned int*)buffer)[0]  = Cmd;
        buffer  += sizeof(unsigned int);
        return insertArguments(buffer, args...);
}

template <typename Result, typename... Args, typename... UArgs>
typename enable_if<length<Args...>::value == length<UArgs...>::value, unsigned char*>::type
unpackAndExecuteCommand(unsigned char* buffer, Result (*fn)(Args...), UArgs*... args)
{
        fn(*args...);
        return buffer;
}

template <typename Result, typename Arg, typename... Args, typename... UArgs>
typename enable_if<length<Arg, Args...>::value != length<UArgs...>::value, unsigned char*>::type
unpackAndExecuteCommand(unsigned char* buffer, Result (*fn)(Args..., Arg), UArgs*... uargs)
{
        Arg*    arg = reinterpret_cast<Arg*>(buffer);
        buffer      += sizeof(Arg);
        return unpackAndExecuteCommand(buffer, fn, arg, uargs...);
}

template <typename Result>
unsigned char* unpackAndExecuteCommand(unsigned char* buffer, Result (*fn)())
{
        fn();
        return buffer;
}

template <typename Result, typename... Args>
unsigned char* executeCommand(unsigned char* buffer, Result (*fn)(Args...))
{
        buffer  += sizeof(unsigned int);
        return unpackAndExecuteCommand(buffer, fn);
}
"""

OPTIMIZED_IMPL = """
inline GLubyte* insertCommand(GLuint cmd, GLubyte* buffer, GLfloat arg0, GLfloat arg1, GLfloat arg2)
{
    ((GLuint*)buffer)[0] = cmd;
    ((GLfloat*)buffer)[1] = arg0;
    ((GLfloat*)buffer)[2] = arg1;
    ((GLfloat*)buffer)[3] = arg2;
    return buffer + sizeof(GLuint)+3*sizeof(GLfloat);
}

inline GLubyte* insertCommand(GLuint cmd, GLubyte* buffer, GLfloat arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3)
{
    ((GLuint*)buffer)[0] = cmd;
    ((GLfloat*)buffer)[1] = arg0;
    ((GLfloat*)buffer)[2] = arg1;
    ((GLfloat*)buffer)[3] = arg2;
    ((GLfloat*)buffer)[4] = arg3;
    return buffer + sizeof(GLuint)+4*sizeof(GLfloat);
}

inline GLubyte* insertCommand(GLuint cmd, GLubyte* buffer, GLfloat arg0, GLfloat arg1, GLfloat arg2, GLfloat arg3, GLuint arg4)
{
    ((GLuint*)buffer)[0] = cmd;
    ((GLfloat*)buffer)[1] = arg0;
    ((GLfloat*)buffer)[2] = arg1;
    ((GLfloat*)buffer)[3] = arg2;
    ((GLfloat*)buffer)[4] = arg3;
    ((GLuint*)buffer)[5] = arg4;
    return buffer + sizeof(GLuint)+4*sizeof(GLfloat)+sizeof(GLuint);
}

"""

def makeCommand(length, which):
        cmd = ""
        argtys  = ["Arg"+str(n) for n in range(length)]

        if which == "insert":
                targtys = []
                targtys.extend(["typename "+arg for arg in argtys])

                fnargs  = ["GLuint cmd", "GLubyte* buffer"]
                fnargs.extend([arg+" "+arg.lower() for arg in argtys])

                if len(targtys) > 0:
                        cmd += "template <" + ", ".join(targtys) + ">\n"
                cmd += "inline GLubyte* insertCommand(" + ", ".join(fnargs) + ")\n{\n"
                cmd += "    ((GLuint*)buffer)[0] = cmd;\n"
                offset=" + sizeof(COMMAND)"
                if len(argtys) > 0:
                        for arg in argtys:
                                cmd += "    (("+arg+"*)(buffer"+offset+"))[0] = "+arg.lower()+";\n"
                                offset += " + sizeof("+arg+")"
                cmd += "    return (buffer "+offset+");\n"
                cmd += "}\n"

        if which == "execute":
                targtys = ["typename Context", "typename Result"]
                targtys.extend(["typename "+arg for arg in argtys])

                fpargs  = ["Context&"]
                fpargs.extend([arg for arg in argtys])
                fnptr   = "Result(*fn)(" + ",".join(fpargs)+")"

                fnargs  = ["Context& ctxt","GLubyte const* buffer", fnptr]

                cmd += "template <" + ", ".join(targtys) + ">\n"
                cmd += "inline const GLubyte* executeCommand(" + ", ".join(fnargs) + ")\n{\n"
                offset=" + sizeof(COMMAND)"
                if len(argtys) > 0:
                        for arg in argtys:
                                cmd += "    auto "+arg.lower()+" = reinterpret_cast<typename std::remove_reference<const "+arg+">::type*>(buffer"+offset+");\n"
                                offset += " + sizeof("+arg+")"
                call    = ["ctxt"]
                call.extend(["*"+arg.lower() for arg in argtys])
                cmd += "    fn(" + ", ".join(call) + ");\n"
                cmd += "    return (buffer "+offset+");\n"
                cmd += "}\n"

        return cmd
#
# executeCommand(g_oglState, &glimInterpret);
#
# template <typename A0, ..., typename Ak>
# void glimInterpret(OGL::State& s, GLuint cmd, A0 a0, ..., Ak ... ak)
# {
#     reinterpret_cast<void(*)(OGL::State&,A0,...,Ak)>(g_imProcTbl[cmd])(s,a0,...,ak);
# }
#

def list_generated_files(output_dir):
        gen_files = [
                '%s/serdeser.hpp' % output_dir,
        ]
        return gen_files

def autogen_file(options):
        output_dir      = options.output_dir
        max_args        = options.max_args

        if not os.path.exists(output_dir):
                os.makedirs(output_dir)

        gen_files = []
        of = "%s/serdeser.hpp" % output_dir
        with open(of, "w") as fn:
                print("#ifndef OGL_SERDESER_HPP", file=fn)
                print("#define OGL_SERDESER_HPP", file=fn)
                print("", file=fn)
                print("#include <type_traits>", file=fn)
                print("", file=fn)

                print("namespace OGL", file=fn)
                print("{", file=fn)
                print("", file=fn)

                print("#if 0 // MSVC doesn't support variadic templates.", file=fn)
                print(CORRECT_IMPL, file=fn)
                print("#endif", file=fn)
                print("", file=fn)

                for i in range(int(max_args)):
                        print(makeCommand(i, "insert"), file=fn)
                for i in range(int(max_args)):
                        print(makeCommand(i, "execute"), file=fn)

                print(OPTIMIZED_IMPL, file=fn)

                print("}", file=fn)
                print("", file=fn)
                print("#endif//OGL_SERDESER_HPP", file=fn)

                gen_files.append(of)

        return gen_files

def main(args):
        parser = ArgumentParser(prog='<python32> deser', usage='%(prog)s [options]', formatter_class=RawTextHelpFormatter)

        parser.add_argument('--output-dir', action='store', dest='output_dir', metavar='PATH',
                help='output directory for the generated files\n[DEFAULT=%(default)s]', default=os.path.dirname(__file__))

        parser.add_argument('--max-args', '-m', action='store', dest='max_args',
                help='max number of cmd args to handle in function signatures')

        parser.add_argument('--verbose', '-v', action='store_true', dest='verbose',
                help='print generated file paths to stdout\n[DEFAULT=%(default)s]', default=False)

        options = parser.parse_args(args)

        gen_files = autogen_file(options)

        if sorted(gen_files) != sorted(list_generated_files(options.output_dir)):
                print("ALERT: Update the list of generated files as the output has changed!")

        if options.verbose:
                for f in sorted(gen_files): print(f)

if __name__ == '__main__':
        main(sys.argv[1:])
