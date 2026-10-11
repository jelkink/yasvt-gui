# Yet Another Simple Vocabulary Trainer

Yet Another Simple Vocabulary Trainer: a GTK 4 graphical vocabulary trainer written in C. It is a port of [yasvt-python](https://github.com/) with the same scoring and test ideas.

While lead by Jos Dornschneider-Elkink, the code is written by AI.

## Features

- Load one or more CSV vocabulary files (comma or semicolon separated) and practise them.
- Four test modes, each switchable on or off in the panel on the right (all modes that are ticked are mixed randomly):
  - **Translate**: see the word, type the translation.
  - **Type the word**: see the word you have to type, and type it out.
  - **Hear and translate**: hear the shown word, type the translation.
  - **Hear and type**: hear the word you have to type, and type it out.
- Language filter in the panel on the left: choose which shown and which typed languages to practise, several at once. By default nothing is filtered.
- Progress is remembered between sessions (see [Progress and word order](#progress-and-word-order)). Without opening a file, the program continues with every word you have loaded before.
- Spoken words (English, Russian, Chinese, Cantonese, Italian, Arabic, Dutch, Latin, Indonesian, French, German, Spanish, Portuguese, Japanese, Korean, Turkish, Polish, Hindi, Greek, Swedish) via Google TTS, cached locally. The word is read out after every answer, except for your native languages (default English and Dutch), which you can change under **Settings**.
- Pinyin shown automatically for Chinese words.
- Buttons: **Speak** (play the current word), **Reverse**, **Shuffle**, **Print list** (with scores), **Clear score** (current word), **Delete word** (for this run only).

## Requirements

- A C compiler, `make`, `pkg-config`
- GTK 4 development files
- `curl` and `mpv` (only for audio)

## Build and install

```bash
make                           # build into build/yasvt-gui
make run                       # build and run
sudo make install              # install to /usr/local, with a desktop entry
make install PREFIX=$HOME/.local   # per-user install, no sudo
make uninstall                 # remove (use the same PREFIX)
```

## Usage

```bash
yasvt-gui [--start N] [--number N] [--new PCT] [FILE...]
```

- `--start N`, `-s N`: first word to use, counted from 1 (default 1)
- `--number N`, `-n N`: number of words to use (default: all)
- `--new PCT`, `-N PCT`: share of never-seen words in the initial order, 0 to 100 (default 20)

Files can also be opened from the **Open…** button. Opening files replaces the current word list. Started without files, the program continues with all words from the files you have opened before.

## Vocabulary files

CSV with a header row naming the two languages, then one word per row. The delimiter, `,` or `;`, is detected per file. By default the **first** column is the language you type and the **second** is the one you are shown; **Reverse** swaps them. An optional third column holds a note shown after a wrong answer.

```csv
Russian,English
возможно,maybe
вероятно,probably
```

In this example you are shown "maybe" and type "возможно". Answers must match exactly (apart from surrounding spaces). Language names (case-insensitive) decide whether audio and pinyin are available. See [examples/](examples/).

## Progress and word order

Every word you open is recorded in `~/.local/share/yasvt-gui/progress.txt`, together with your last 8 results for it, when you last answered it, and how many days it takes to become due again. The record is separate for each direction (for example Russian→English and English→Russian). It is updated after each answer.

This record only decides the **initial order** after opening a file, starting without a file, or pressing **Reverse** or changing the language filter:

- Words you have practised come first when they are poorly known or due again; well-known words that were seen recently go to the back. The interval before a word is due doubles after a correct answer (at most once a day, up to a year) and resets after a wrong one. Some randomness keeps the order from being predictable.
- Words you have never answered are mixed in at a fixed rate, 20% by default (`--new`).
- **Shuffle** ignores the record and shuffles completely at random.

After that, the order during the session follows the in-session score: a word you answer comes back later when it is well known and sooner when it is not. The in-session score starts from zero for every run, and **Reverse** resets it.

The progress file is plain text, one word per line. Deleting it resets all progress.

## Audio

Clips are downloaded from Google Translate TTS with `curl` and played with `mpv`. They are cached in `~/.cache/yasvt-gui/`, so each word is only downloaded once. Audio needs an internet connection the first time a word is played. After a correct answer the word you typed is read out; after a wrong answer both words are, one after the other. Languages marked as native under **Settings** are not read out automatically, and the hear modes are not used for them. The **Speak** button always plays the current word.

## Files

| Path | Contents |
|---|---|
| `~/.local/share/yasvt-gui/progress.txt` | learning progress and the words you have loaded |
| `~/.config/yasvt-gui/settings.ini` | native languages |
| `~/.cache/yasvt-gui/` | downloaded audio clips (safe to delete) |

## Development

`src/pinyin_table.c` is generated by `tools/gen_pinyin.py`, which needs the `pypinyin` Python package:

```bash
python3 tools/gen_pinyin.py > src/pinyin_table.c
```

## License

GNU General Public License v3.0, see [LICENSE](LICENSE).
