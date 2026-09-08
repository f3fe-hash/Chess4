from __future__ import annotations

import os
from pathlib import Path

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
		last_move: str | None = None,
		evaluation: str | None = None,
	) -> str:
		"""
		Generate a chat reply.

		last_move:
			The most recent move played, in UCI notation.

		evaluation:
			The engine's evaluation of the current position.
			Examples: "+35", "-120", "mate +3".
		"""

		context = ""

		if last_move is not None:
			context += f"Last move: {last_move}\n"

		if evaluation is not None:
			context += f"Position evaluation: {evaluation}\n"

		if context:
			context += "\n"

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