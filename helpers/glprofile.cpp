/**************************************************************************
 *
 * Copyright 2014 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/


#include "glprofile.hpp"

#include <assert.h>

#include "os.hpp"
#include "glproc.hpp"


namespace glprofile {


std::ostream &
operator << (std::ostream &os, const Profile & profile) {
    os << "OpenGL";
    if (profile.api == API_GLES) {
        os << " ES";
    }
    os << " " << profile.major << "." << profile.minor;
    if (profile.api == API_GL &&
        profile.versionGreaterOrEqual(3, 2)) {
        os << " " << (profile.core ? "core" : "compat");
    }
    return os;
}


static inline bool
isDigit(char c) {
    return c >= '0' && c <= '9';
}


static unsigned
parseNumber(const char * & p)
{
    unsigned n = 0;
    char c = *p;
    while (isDigit(c)) {
        unsigned digit = c - '0';
        n *= 10;
        n += digit;
        ++p;
        c = *p;
    }
    return n;
}


/*
 * Parse API and version numbers from a GL_VERSION string.
 *
 * OpenGL 2.1 specification states that GL_VERSION string is laid out as
 *
 *     <version number><space><vendor-specific information>
 *
 *   Where <version number> is either of the form <major number>.<minor number>
 *   or <major number>.<minor number>.<release number>, where the numbers all
 *   have one or more digits.  The release number and vendor specific
 *   information are optional.
 *
 * OpenGL ES 1.x specification states that GL_VERSION is laid out as
 *
 *   "OpenGL ES-XX 1.x" XX={CM,CL}
 *
 * OpenGL ES 2 and 3 specifications state that GL_VERSION is laid out as
 *
 *   "OpenGL ES N.M vendor-specific information"
 */
static Profile
parseVersion(const char *version)
{
    Profile profile(API_GL, 0, 0, false);

    const char *p = version;

    if (p[0] == 'O' &&
        p[1] == 'p' &&
        p[2] == 'e' &&
        p[3] == 'n' &&
        p[4] == 'G' &&
        p[5] == 'L' &&
        p[6] == ' ' &&
        p[7] == 'E' &&
        p[8] == 'S') {
        p += 9;

        profile.api = API_GLES;

        // skip `-{CM,CL}`
        if (*p == '-') {
            ++p;
            while (*p != ' ') {
                if (*p == '\0') {
                    goto malformed;
                }
                ++p;
            }
        }

        // skip white-space
        while (*p == ' ') {
            if (*p == '\0') {
                goto malformed;
            }
            ++p;
        }
    }

    if (!isDigit(*p)) {
        goto malformed;
    }

    profile.major = parseNumber(p);
    if (*p++ == '.' &&
        isDigit(*p)) {
        profile.minor = parseNumber(p);
    } else {
        goto malformed;
    }

    return profile;

malformed:
    os::log("warning: malformed GL_VERSION (\"%s\")\n", version);
    return profile;
}


/*
 * Get the profile of the current context.
 */
Profile
getCurrentContextProfile(void)
{
    Profile profile(API_GL, 0, 0, false);

    assert(parseVersion("3.0 Mesa 10.3.2") == Profile(API_GL, 3, 0));
    assert(parseVersion("3.3 (Core Profile) Mesa 10.3.2") == Profile(API_GL, 3, 3));
    assert(parseVersion("4.4.0 NVIDIA 331.89") == Profile(API_GL, 4, 4));
    assert(parseVersion("OpenGL ES 3.0 Mesa 10.3.2") == Profile(API_GLES, 3, 0));

    const char *version = (const char *)_glGetString(GL_VERSION);
    if (!version) {
        os::log("warning: null GL_VERSION\n");
        return profile;
    }

    // Parse the version string.
    profile = parseVersion(version);

    if (profile.major >= 3) {
        /*
         * From OpenGL and OpenGL ES version 3 onwards, the GL version may also
         * be queried via GL_MAJOR VERSION and GL_MINOR_VERSION, which should
         * match the major number and minor number in GL_VERSION string, so use
         * it to check we parsed the versions correcly.
         */

        GLint majorVersion = 0;
        _glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
        GLint minorVersion = 0;
        _glGetIntegerv(GL_MINOR_VERSION, &minorVersion);

        if (majorVersion != profile.major ||
            minorVersion != profile.minor) {
            os::log("apitrace: warning: OpenGL context version mismatch (GL_VERSION=\"%s\", but GL_MAJOR/MINOR_VERSION=%u.%u)\n",
                    version, majorVersion, minorVersion);
        }
    }

    if (profile.api == API_GL &&
        profile.versionGreaterOrEqual(3, 2)) {
        GLint profileMask = 0;
        _glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profileMask);
        if (profileMask & GL_CONTEXT_CORE_PROFILE_BIT) {
            profile.core = true;
        }
    }

    return profile;

}


} /* namespace glprofile */
