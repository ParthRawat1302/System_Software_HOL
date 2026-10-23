import { useState } from "react";
import { api } from "./api";

export default function LoginPage({ onLoggedIn }) {
  const [kind, setKind] = useState("USER");
  const [id, setId] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);

  async function handleSubmit(e) {
    e.preventDefault();
    setError("");
    setBusy(true);
    try {
      const result = await api.login(kind, id, password);
      api.setSessionToken(result.session_token);
      onLoggedIn(kind, result.staff_role);
    } catch (err) {
      setError(err.message);
    } finally {
      setBusy(false);
    }
  }

  const idLabel = kind === "ADMIN" ? "Admin ID" : kind === "USER" ? "Account ID" : `${kind[0]}${kind.slice(1).toLowerCase()} ID`;

  return (
    <div className="card">
      <h1>Bank Login</h1>
      <form onSubmit={handleSubmit}>
        <label>
          Login as
          <select value={kind} onChange={(e) => setKind(e.target.value)}>
            <option value="USER">Customer (normal or joint account)</option>
            <option value="EMPLOYEE">Employee</option>
            <option value="MANAGER">Manager</option>
            <option value="ADMIN">Administrator</option>
          </select>
        </label>
        <label>
          {idLabel} (numeric, e.g. 1)
          <input
            value={id}
            onChange={(e) => setId(e.target.value)}
            inputMode="numeric"
            placeholder="1"
            required
          />
        </label>
        <label>
          Password
          <input type="password" value={password} onChange={(e) => setPassword(e.target.value)} required />
        </label>
        {error && <p className="error">{error}</p>}
        <button type="submit" disabled={busy}>
          {busy ? "Logging in..." : "Login"}
        </button>
      </form>
    </div>
  );
}
