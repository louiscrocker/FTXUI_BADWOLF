"""Fallback setup.py for environments that do not support PEP 517."""

from setuptools import setup, find_packages

setup(
    name="pyftxui",
    version="0.1.0",
    packages=find_packages(),
    package_data={"pyftxui": ["*.pyi", "*.so", "*.pyd"]},
    python_requires=">=3.8",
)
