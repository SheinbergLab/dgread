function build_dgread()
%BUILD_DGREAD Build the dg_read MEX file
%
%   build_dgread()
%
%   Compiles the dg_read MEX file for the current platform.
%   Requires MATLAB R2018a+ for C++ MEX API.
%   Requires a C++ compiler configured with mex -setup C++.
%
%   The C core (../src/core, ../src/lz4, ../src/zlib) is compiled to object
%   files with the C compiler first, then linked against the C++ MEX wrapper.
%   Compiling those .c files in one mex call with dg_read.cpp fails: mex picks
%   a single compiler for the whole source list, so the C sources would be fed
%   to clang++/cl as C++ (e.g. 'new' is used as an identifier in dfutils.c).

here = fileparts(mfilename('fullpath'));
root = fileparts(here);

core_dir = fullfile(root, 'src', 'core');
lz4_dir  = fullfile(root, 'src', 'lz4');
zlib_dir = fullfile(root, 'src', 'zlib');

% C sources: dg core + lz4 + vendored zlib (compiled in, so no external
% zlib is needed on any platform -- matches the Python/R builds).
c_sources = [ ...
    fullfile(core_dir, {'df.c', 'dfutils.c', 'dynio.c', 'flipfuncs.c', 'lz4utils.c'}), ...
    fullfile(lz4_dir,  {'lz4.c', 'lz4hc.c', 'lz4frame.c', 'xxhash.c'}), ...
    fullfile(zlib_dir, {'adler32.c', 'compress.c', 'crc32.c', 'deflate.c', ...
                        'gzclose.c', 'gzlib.c', 'gzread.c', 'gzwrite.c', ...
                        'infback.c', 'inffast.c', 'inflate.c', 'inftrees.c', ...
                        'trees.c', 'uncompr.c', 'zutil.c'})];

% Include directories: zlib first so the vendored zlib.h is used.
includes = {['-I' zlib_dir], ['-I' core_dir], ['-I' lz4_dir]};

defines = {};
if ~ispc
    % zlib's gz*.c need lseek/read/write/close from <unistd.h>, which zlib
    % only includes when Z_HAVE_UNISTD_H is set (normally by ./configure).
    % We vendor the unconfigured zconf.h, so define it here.
    defines = {'-DZ_HAVE_UNISTD_H=1'};
end

objdir = fullfile(here, 'build');
if exist(objdir, 'dir')
    % Reuse (and keep) a directory we did not create.
    cleanup = [];
else
    mkdir(objdir);
    cleanup = onCleanup(@() rmdir(objdir, 's'));
end

fprintf('Compiling C sources...\n');
mex('-c', '-outdir', objdir, includes{:}, defines{:}, c_sources{:});

if ispc
    objext = '.obj';
else
    objext = '.o';
end
objs = cell(size(c_sources));
for i = 1:numel(c_sources)
    [~, base] = fileparts(c_sources{i});
    objs{i} = fullfile(objdir, [base objext]);
end

fprintf('Linking dg_read MEX file (C++ API)...\n');
mex('-R2018a', '-output', fullfile(here, 'dg_read'), ...
    includes{:}, fullfile(here, 'dg_read.cpp'), objs{:});

fprintf('Done! dg_read.%s created.\n', mexext);
end
