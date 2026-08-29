"""Run the current placeholder end-to-end demo."""

from pprint import pprint

from denso_d2.integration import run_vertical_slice


def main() -> None:
    pprint(run_vertical_slice())


if __name__ == "__main__":
    main()
