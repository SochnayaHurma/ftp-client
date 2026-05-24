from __future__ import annotations

import argparse
import shutil
from pathlib import Path


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def reset_directory(path: Path) -> None:
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True, exist_ok=True)


def build_demo_data(root: Path) -> None:
    local = root / "local_root"
    server = root / "server_root"
    reset_directory(local)
    reset_directory(server)

    write_text(local / "01_new" / "new_report.txt", "Новый отчёт, которого ещё нет на сервере.\n")

    same_payload = "Содержимое одинаковое в локальной и серверной папке.\n"
    write_text(local / "02_same" / "same.txt", same_payload)
    write_text(server / "02_same" / "same.txt", same_payload)

    write_text(local / "03_overwrite" / "contract.txt", "Версия договора 2.0.\n")
    write_text(server / "03_overwrite" / "contract.txt", "Версия договора 1.0.\n")

    write_text(local / "04_conflict" / "archive", "Это файл, но на сервере по этому пути папка.\n")
    (server / "04_conflict" / "archive").mkdir(parents=True, exist_ok=True)
    write_text(server / "04_conflict" / "archive" / "nested.txt", "Файл внутри конфликтующей папки.\n")

    write_text(local / "05_project" / "README.md", "# Demo project\n")
    write_text(local / "05_project" / "src" / "main.cpp", "int main() { return 0; }\n")
    write_text(local / "05_project" / "docs" / "manual.txt", "Краткое руководство.\n")

    write_text(local / "06_filter" / "notes.md", "Заметка Markdown.\n")
    write_text(local / "06_filter" / "image.png", "PNG-заглушка для фильтра по расширению.\n")
    write_text(server / "06_filter" / "old_notes.md", "Старая заметка на сервере.\n")


def main() -> None:
    parser = argparse.ArgumentParser(description="Prepare SecureTransfer Lite demo folders")
    parser.add_argument(
        "--root",
        default=str(Path(__file__).resolve().parents[1] / "demo_data"),
        help="Каталог, в котором будут созданы local_root и server_root",
    )
    args = parser.parse_args()

    root = Path(args.root).resolve()
    build_demo_data(root)
    print(f"Demo data prepared: {root}")
    print(f"Local root:  {root / 'local_root'}")
    print(f"Server root: {root / 'server_root'}")


if __name__ == "__main__":
    main()
