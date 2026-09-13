from __future__ import annotations

import os
from pathlib import Path
from typing import Optional

from groq import Groq


GROQ_MODEL = os.environ.get("GROQ_MODEL", "openai/gpt-oss-20b")
MAX_HISTORY_MESSAGES = 12
MAX_REPLY_LENGTH = 140
DEFAULT_SYSTEM_MESSAGE_FILE = Path(__file__).with_name("system_message.txt")


def load_system_message() -> str:
	path = Path(
		os.environ.get(
			"GROQ_SYSTEM_MESSAGE_FILE",
			str(DEFAULT_SYSTEM_MESSAGE_FILE),
		)
	)

	try:
		message = path.read_text(encoding="utf-8").strip()
	except OSError as exc:
		raise RuntimeError(
			f"Could not read system message file {path}: {exc}"
		) from exc

	if not message:
		raise ValueError(f"System message file {path} is empty")

	return message


class GroqChat:
	"""Generate concise replies for a single Lichess game's chat."""

	def __init__(self, api_key: str, model: str = GROQ_MODEL):
		if not api_key:
			raise ValueError("GROQ_API_KEY is not set")

		self.model = model
		self.client = Groq(api_key=api_key, timeout=30.0)

		self.messages: list[dict[str, str]] = [
			{
				"role": "system",
				"content": load_system_message(),
			}
		]

	def reply(
		self,
		username: str,
		text: str,
		*,
		board_fen: Optional[str] = None,
		board_ascii: Optional[str] = None,
		bot_color: Optional[str] = None,
		side_to_move: Optional[str] = None,
		last_move: Optional[str] = None,
		evaluation: Optional[str] = None,
		move_number: Optional[int] = None,
		material: Optional[str] = None,
		status: Optional[str] = None,
		in_check: Optional[bool] = None,
		legal_moves: Optional[list[str]] = None,
		recent_moves: Optional[list[str]] = None,
	) -> str:
		"""
		Generate a chat reply using the current chess position.

		board_fen:
			Current position in FEN notation.

		board_ascii:
			Human-readable ASCII representation of the board.

		bot_color:
			The color the bot is playing.

		side_to_move:
			The color whose turn it currently is.

		last_move:
			The most recent move in UCI notation.

		evaluation:
			Engine evaluation in pawns, e.g. "+1.24", "-2.64",
			or "mate +3".

		move_number:
			Current full-move number.

		material:
			Material summary such as "White: Q R R B N, Black: Q R R B N".

		status:
			Current game status.

		in_check:
			Whether the side to move is currently in check.

		legal_moves:
			Legal moves in UCI notation.

		recent_moves:
			Recent moves in UCI notation.
		"""

		context_parts: list[str] = []

		if bot_color is not None:
			context_parts.append(f"Bot side: {bot_color}")

		if side_to_move is not None:
			context_parts.append(f"Side to move: {side_to_move}")

		if move_number is not None:
			context_parts.append(f"Move number: {move_number}")

		if last_move is not None:
			context_parts.append(f"Last move: {last_move}")

		if evaluation is not None:
			context_parts.append(
				f"Engine evaluation: {evaluation}"
			)

		if material is not None:
			context_parts.append(f"Material: {material}")

		if status is not None:
			context_parts.append(f"Game status: {status}")

		if in_check is not None:
			context_parts.append(
				f"Side to move is in check: "
				f"{'yes' if in_check else 'no'}"
			)

		if recent_moves:
			context_parts.append(
				"Recent moves: " + " ".join(recent_moves)
			)

		if board_fen is not None:
			context_parts.append(
				"Current FEN:\n" + board_fen
			)

		if board_ascii is not None:
			context_parts.append(
				"Current board:\n" + board_ascii
			)

		if legal_moves:
			context_parts.append(
				"Legal moves: " + " ".join(legal_moves)
			)

		context = ""

		if context_parts:
			context = (
				"[CURRENT CHESS POSITION]\n"
				+ "\n".join(context_parts)
				+ "\n\n"
			)

		self.messages.append({
			"role": "user",
			"content": (
				f"{context}"
				f"{username} says: {text}"
			),
		})

		completion = self.client.chat.completions.create(
			model=self.model,
			messages=self.messages,
			temperature=0.7,
			max_completion_tokens=256,
			reasoning_effort="low",
		)

		choices = completion.choices

		if not choices:
			raise RuntimeError("Groq returned no chat choices")

		reply = choices[0].message.content

		if not isinstance(reply, str) or not reply.strip():
			raise RuntimeError("Groq returned an empty chat reply")

		reply = " ".join(reply.split())[:MAX_REPLY_LENGTH]

		self.messages.append({
			"role": "assistant",
			"content": reply,
		})

		if len(self.messages) > MAX_HISTORY_MESSAGES + 1:
			self.messages = [
				self.messages[0]
			] + self.messages[-MAX_HISTORY_MESSAGES:]

		return reply
