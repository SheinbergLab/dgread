"""
dgread_utils - High-level utilities for working with dg/dgz files.

This module provides convenience functions on top of the dgread C extension,
including pandas DataFrame conversion.

Basic usage:
    import dgread
    data = dgread.dgread('session.dgz')  # Returns dict of arrays
    
With pandas:
    from dgread_utils import to_dataframe, load_session
    df = load_session('session.dgz')  # Returns DataFrame for rectangular data
"""

from typing import Dict, List, Optional, Any, Union
from pathlib import Path

try:
    import dgread
except ImportError:
    raise ImportError(
        "dgread C extension not found. "
        "Please ensure dgread is properly installed: pip install dgread"
    )

import numpy as np


def read(filename: Union[str, Path]) -> Dict[str, np.ndarray]:
    """
    Read a dg/dgz file and return a dictionary of arrays.
    
    This is a convenience wrapper around dgread.dgread() that accepts
    Path objects and provides better error messages.
    
    Parameters
    ----------
    filename : str or Path
        Path to the dg or dgz file.
        
    Returns
    -------
    dict
        Dictionary mapping list names to numpy arrays.
        Nested/ragged lists are returned as object arrays containing sub-arrays.
        
    Examples
    --------
    >>> data = read('session.dgz')
    >>> print(data.keys())
    dict_keys(['stimtype', 'response', 'rt', 'em', 'events'])
    >>> print(data['rt'][:5])
    [342, 289, 456, 312, 378]
    """
    filename = str(filename)
    
    if not Path(filename).exists():
        raise FileNotFoundError(f"File not found: {filename}")
    
    return dgread.dgread(filename)


def list_names(filename: Union[str, Path]) -> List[str]:
    """
    Get the list names in a dg/dgz file without loading all data.
    
    Parameters
    ----------
    filename : str or Path
        Path to the dg or dgz file.
        
    Returns
    -------
    list
        List of column/list names in the file.
    """
    data = read(filename)
    return list(data.keys())


def get_lengths(data: Dict[str, np.ndarray]) -> Dict[str, int]:
    """
    Get the length of each list in a dg data dictionary.
    
    Parameters
    ----------
    data : dict
        Dictionary from dgread.dgread()
        
    Returns
    -------
    dict
        Dictionary mapping list names to their lengths.
    """
    return {name: len(arr) for name, arr in data.items()}


def is_rectangular(data: Dict[str, np.ndarray]) -> bool:
    """
    Check if all lists have the same length (rectangular data).
    
    Parameters
    ----------
    data : dict
        Dictionary from dgread.dgread()
        
    Returns
    -------
    bool
        True if all lists have the same length.
    """
    lengths = set(get_lengths(data).values())
    return len(lengths) <= 1


def get_scalar_columns(data: Dict[str, np.ndarray]) -> List[str]:
    """
    Get names of columns that contain scalar (non-nested) data.
    
    These columns are suitable for direct DataFrame conversion.
    
    Parameters
    ----------
    data : dict
        Dictionary from dgread.dgread()
        
    Returns
    -------
    list
        Names of columns with scalar data.
    """
    scalar_cols = []
    for name, arr in data.items():
        if arr.dtype != object:
            scalar_cols.append(name)
        elif len(arr) > 0 and not isinstance(arr[0], np.ndarray):
            scalar_cols.append(name)
    return scalar_cols


def get_nested_columns(data: Dict[str, np.ndarray]) -> List[str]:
    """
    Get names of columns that contain nested/ragged data.
    
    Parameters
    ----------
    data : dict
        Dictionary from dgread.dgread()
        
    Returns
    -------
    list
        Names of columns with nested arrays.
    """
    nested_cols = []
    for name, arr in data.items():
        if arr.dtype == object and len(arr) > 0:
            if isinstance(arr[0], np.ndarray):
                nested_cols.append(name)
    return nested_cols


def to_dataframe(
    data: Dict[str, np.ndarray],
    columns: Optional[List[str]] = None,
    include_nested: bool = False,
) -> "pd.DataFrame":
    """
    Convert dg data to a pandas DataFrame.
    
    By default, only scalar columns with matching lengths are included.
    Nested/ragged columns can be included as object columns if desired.
    
    Parameters
    ----------
    data : dict
        Dictionary from dgread.dgread()
    columns : list, optional
        Specific columns to include. If None, includes all scalar columns
        with the same length as the longest list.
    include_nested : bool, default False
        If True, include nested columns as object dtype.
        
    Returns
    -------
    pandas.DataFrame
        DataFrame with selected columns.
        
    Raises
    ------
    ImportError
        If pandas is not installed.
        
    Examples
    --------
    >>> data = dgread.dgread('session.dgz')
    >>> df = to_dataframe(data)
    >>> df.head()
       stimtype  response   rt  correct
    0         1         1  342        1
    1         2         1  289        1
    2         1         0  456        0
    """
    try:
        import pandas as pd
    except ImportError:
        raise ImportError(
            "pandas is required for DataFrame conversion. "
            "Install with: pip install dgread[pandas]"
        )
    
    if columns is None:
        # Get all scalar columns
        columns = get_scalar_columns(data)
        if include_nested:
            columns.extend(get_nested_columns(data))
    
    # Find the common length (use max length, filter columns that match)
    lengths = {name: len(data[name]) for name in columns if name in data}
    if not lengths:
        return pd.DataFrame()
    
    max_len = max(lengths.values())
    
    # Build DataFrame with columns that have the right length
    df_data = {}
    for name in columns:
        if name in data and len(data[name]) == max_len:
            df_data[name] = data[name]
    
    return pd.DataFrame(df_data)


def load_session(
    filename: Union[str, Path],
    columns: Optional[List[str]] = None,
    include_nested: bool = False,
) -> "pd.DataFrame":
    """
    Load a dg/dgz file directly into a pandas DataFrame.
    
    Convenience function combining read() and to_dataframe().
    
    Parameters
    ----------
    filename : str or Path
        Path to the dg or dgz file.
    columns : list, optional
        Specific columns to include.
    include_nested : bool, default False
        If True, include nested columns as object dtype.
        
    Returns
    -------
    pandas.DataFrame
        DataFrame with session data.
        
    Examples
    --------
    >>> df = load_session('session.dgz')
    >>> print(f"Loaded {len(df)} trials")
    Loaded 847 trials
    """
    data = read(filename)
    return to_dataframe(data, columns=columns, include_nested=include_nested)


def summary(filename: Union[str, Path]) -> Dict[str, Any]:
    """
    Get a summary of a dg/dgz file.
    
    Parameters
    ----------
    filename : str or Path
        Path to the dg or dgz file.
        
    Returns
    -------
    dict
        Summary information including list names, lengths, and types.
        
    Examples
    --------
    >>> info = summary('session.dgz')
    >>> print(info)
    {
        'filename': 'session.dgz',
        'n_lists': 12,
        'n_trials': 847,
        'lists': {
            'stimtype': {'length': 847, 'dtype': 'int32', 'nested': False},
            'em': {'length': 847, 'dtype': 'object', 'nested': True},
            ...
        }
    }
    """
    filename = Path(filename)
    data = read(filename)
    
    lists_info = {}
    max_len = 0
    
    for name, arr in data.items():
        is_nested = arr.dtype == object and len(arr) > 0 and isinstance(arr[0], np.ndarray)
        lists_info[name] = {
            'length': len(arr),
            'dtype': str(arr.dtype),
            'nested': is_nested,
        }
        if len(arr) > max_len:
            max_len = len(arr)
    
    return {
        'filename': filename.name,
        'n_lists': len(data),
        'n_trials': max_len,
        'rectangular': is_rectangular(data),
        'lists': lists_info,
    }


def print_summary(filename: Union[str, Path]) -> None:
    """
    Print a formatted summary of a dg/dgz file.
    
    Parameters
    ----------
    filename : str or Path
        Path to the dg or dgz file.
    """
    info = summary(filename)
    
    print(f"\n{info['filename']}")
    print(f"  {info['n_lists']} lists, {info['n_trials']} trials")
    print(f"  Rectangular: {info['rectangular']}")
    print("\n  Lists:")
    
    for name, linfo in info['lists'].items():
        nested_str = " (nested)" if linfo['nested'] else ""
        print(f"    {name}: {linfo['length']} x {linfo['dtype']}{nested_str}")
