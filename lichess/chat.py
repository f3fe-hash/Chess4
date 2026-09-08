from __future__ import annotations

import os

from groq import Groq


GROQ_MODEL = os.environ.get("GROQ_MODEL", "openai/gpt-oss-20b")
MAX_HISTORY_MESSAGES = 12
MAX_REPLY_LENGTH = 140


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
				"content": (
					"You are a friendly chess bot chatting on Lichess. "
					"Reply naturally and briefly, in 140 characters or "
					"fewer. Do not claim to be human."
				),
			}
		]

	def reply(self, username: str, text: str) -> str:
		self.messages.append({
			"role": "user",
			"content": f"{username} says: {text}",
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
			self.messages = [self.messages[0]] + self.messages[-MAX_HISTORY_MESSAGES:]

		return reply
