// Copyright 2014 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

struct GLTraceNone
{
};
static const GLTraceNone glTraceNone = GLTraceNone();

inline void print(State &s, GLTraceNone const &)
{
}

inline void print(State &s, GLsizeiptrARB d)
{
    fprintf(s.mpLog, "\t%td", d);
}

inline void print(State &s, GLboolean b)
{
    fprintf(s.mpLog, "\t%d", b);
}

inline void print(State &s, GLint i)
{
    fprintf(s.mpLog, "\t%d", i);
}

inline void print(State &s, OGL::EnumTy e)
{
    if (GetEnumTyMap().find(e) != GetEnumTyMap().end())
    {
        fprintf(s.mpLog, "\t%s", GetEnumTyMap().find(e)->second);
    }
    else
    {
        fprintf(s.mpLog, "\tUndefined");
    }
}

inline void print(State &s, OGL::BitfieldTy e)
{
    fprintf(s.mpLog, "\t0x%x", e);
}

inline void print(State &s, GLuint e)
{
    fprintf(s.mpLog, "\t%d", e);
}

inline void print(State &s, GLfloat f)
{
    fprintf(s.mpLog, "\t%f", f);
}

inline void print(State &s, const GLvoid *ptr)
{
    fprintf(s.mpLog, "\t0x%016llx", (long long)ptr);
}

inline void print(State &s, GLdouble d)
{
    fprintf(s.mpLog, "\t%f", d);
}

inline void print(State &s, SWRL::v4f const &v)
{
    fprintf(s.mpLog, "\t<%f %f %f %f>", v[0], v[1], v[2], v[3]);
}

inline void print(State &s, SWRL::m44f const &m)
{
    fprintf(s.mpLog, "\n\t+-%63s-+\n", "");
    fprintf(s.mpLog, "\t| % 15.8f % 15.8f % 15.8f % 15.8f |\n", m[0][0], m[0][1], m[0][2], m[0][3]);
    fprintf(s.mpLog, "\t| % 15.8f % 15.8f % 15.8f % 15.8f |\n", m[1][0], m[1][1], m[1][2], m[1][3]);
    fprintf(s.mpLog, "\t| % 15.8f % 15.8f % 15.8f % 15.8f |\n", m[2][0], m[2][1], m[2][2], m[2][3]);
    fprintf(s.mpLog, "\t| % 15.8f % 15.8f % 15.8f % 15.8f |\n", m[3][0], m[3][1], m[3][2], m[3][3]);
    fprintf(s.mpLog, "\t+-%63s-+", "");
}

template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10, typename T11>
inline void glTrace(const char *pName, State &s, T0 const &t0, T1 const &t1, T2 const &t2, T3 const &t3, T4 const &t4, T5 const &t5, T6 const &t6, T7 const &t7, T8 const &t8, T9 const &t9, T10 const &t10, T11 const &t11)
{
    if (!s.mpLog)
        return;
    fprintf(s.mpLog, "%s", pName);
    print(s, t0);
    print(s, t1);
    print(s, t2);
    print(s, t3);
    print(s, t4);
    print(s, t5);
    print(s, t6);
    print(s, t7);
    print(s, t8);
    print(s, t9);
    print(s, t10);
    print(s, t11);
    fprintf(s.mpLog, "%s", "\n");
}

inline void glTrace(const char *pName, State &s)
{
    glTrace(pName, s, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone);
}

template <typename T0>
inline void glTrace(const char *pName, State &s, T0 const &t0)
{
    glTrace(pName, s, t0, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone);
}

template <typename T0, typename T1>
inline void glTrace(const char *pName, State &s, T0 const &t0, T1 const &t1)
{
    glTrace(pName, s, t0, t1, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone);
}

template <typename T0, typename T1, typename T2>
inline void glTrace(const char *pName, State &s, T0 const &t0, T1 const &t1, T2 const &t2)
{
    glTrace(pName, s, t0, t1, t2, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone);
}

template <typename T0, typename T1, typename T2, typename T3>
inline void glTrace(const char *pName, State &s, T0 const &t0, T1 const &t1, T2 const &t2, T3 const &t3)
{
    glTrace(pName, s, t0, t1, t2, t3, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone);
}

template <typename T0, typename T1, typename T2, typename T3, typename T4>
inline void glTrace(const char *pName, State &s, T0 const &t0, T1 const &t1, T2 const &t2, T3 const &t3, T4 const &t4)
{
    glTrace(pName, s, t0, t1, t2, t3, t4, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone);
}

template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
inline void glTrace(const char *pName, State &s, T0 const &t0, T1 const &t1, T2 const &t2, T3 const &t3, T4 const &t4, T5 const &t5)
{
    glTrace(pName, s, t0, t1, t2, t3, t4, t5, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone);
}

template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
inline void glTrace(const char *pName, State &s, T0 const &t0, T1 const &t1, T2 const &t2, T3 const &t3, T4 const &t4, T5 const &t5, T6 const &t6)
{
    glTrace(pName, s, t0, t1, t2, t3, t4, t5, t6, glTraceNone, glTraceNone, glTraceNone, glTraceNone, glTraceNone);
}

template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
inline void glTrace(const char *pName, State &s, T0 const &t0, T1 const &t1, T2 const &t2, T3 const &t3, T4 const &t4, T5 const &t5, T6 const &t6, T7 const &t7)
{
    glTrace(pName, s, t0, t1, t2, t3, t4, t5, t6, t7, glTraceNone, glTraceNone, glTraceNone, glTraceNone);
}

template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
inline void glTrace(const char *pName, State &s, T0 const &t0, T1 const &t1, T2 const &t2, T3 const &t3, T4 const &t4, T5 const &t5, T6 const &t6, T7 const &t7, T8 const &t8)
{
    glTrace(pName, s, t0, t1, t2, t3, t4, t5, t6, t7, t8, glTraceNone, glTraceNone, glTraceNone);
}

template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
inline void glTrace(const char *pName, State &s, T0 const &t0, T1 const &t1, T2 const &t2, T3 const &t3, T4 const &t4, T5 const &t5, T6 const &t6, T7 const &t7, T8 const &t8, T9 const &t9)
{
    glTrace(pName, s, t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, glTraceNone, glTraceNone);
}

template <typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>
inline void glTrace(const char *pName, State &s, T0 const &t0, T1 const &t1, T2 const &t2, T3 const &t3, T4 const &t4, T5 const &t5, T6 const &t6, T7 const &t7, T8 const &t8, T9 const &t9, T10 const &t10)
{
    glTrace(pName, s, t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, glTraceNone);
}
