function build_dgread()
%BUILD_DGREAD Build the dg_read MEX file
%
%   build_dgread()
%
%   Compiles the dg_read MEX file for the current platform.
%   Requires MATLAB R2018a+ for C++ MEX API.
%   Requires a C++ compiler configured with mex -setup C++.

% Source files
src_dir = fullfile('..', 'src', 'core');
lz4_dir = fullfile('..', 'src', 'lz4');

sources = {
    'dg_read.cpp'
    fullfile(src_dir, 'df.c')
    fullfile(src_dir, 'dfutils.c')
    fullfile(src_dir, 'dynio.c')
    fullfile(src_dir, 'flipfuncs.c')
    fullfile(src_dir, 'lz4utils.c')
    fullfile(lz4_dir, 'lz4.c')
    fullfile(lz4_dir, 'lz4hc.c')
    fullfile(lz4_dir, 'lz4frame.c')
    fullfile(lz4_dir, 'xxhash.c')
};

% Include directories
includes = {
    ['-I' src_dir]
    ['-I' lz4_dir]
};

% Platform-specific options
if ispc
    % Windows - may need bundled zlib
    libs = {'-lz'};
    if exist('zlib64.lib', 'file')
        libs = {'zlib64.lib'};
    end
else
    % macOS / Linux
    libs = {'-lz'};
end

% Build command - use C++ API
fprintf('Building dg_read MEX file (C++ API)...\n');
args = [sources; includes; libs; {'-R2018a'}];
mex(args{:});

fprintf('Done! dg_read.%s created.\n', mexext);
end
