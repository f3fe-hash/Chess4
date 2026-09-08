from __future__ import annotations

import os

import requests


GROQ_API_URL = "https://api.groq.com/openai/v1/chat/completions"
GROQ_MODEL = os.environ.get("GROQ_MODEL", "openai/gpt-oss-20b")
MAX_HISTORY_MESSAGES = 12
MAX_REPLY_LENGTH = 140


class GroqChat:
	"""Generate concise replies for a single Lichess game's chat."""

	def __init__(self, api_key: str, model: str = GROQ_MODEL):
		if not api_key:
			raise ValueError("GROQ_API_KEY is not set")

		self.model = model
		self.session = requests.Session()
		self.session.headers.update({
			"Authorization": f"Bearer {api_key}",
			"Content-Type": "application/json",
			"User-Agent": "CustomUCIChessBot/1.0",
		})
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

		response = self.session.post(
			GROQ_API_URL,
			json={
				"model": self.model,
				"messages": self.messages,
				"temperature": 0.7,
				"max_tokens": 80,
			},
			timeout=30,
		)
		response.raise_for_status()

		data = response.json()
		choices = data.get("choices", [])
		if not choices:
			raise RuntimeError("Groq returned no chat choices")

		message = choices[0].get("message", {})
		reply = message.get("content")
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
