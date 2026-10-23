import { useState } from "react";
import { api } from "./api";

const MENU = [
  { key: "deposit", label: "Deposit" },
  { key: "withdraw", label: "Withdraw" },
  { key: "transfer", label: "Transfer Funds" },
  { key: "balance", label: "Balance Enquiry" },
  { key: "loan_apply", label: "Apply for a Loan" },
  { key: "loan_list", label: "My Loan Applications" },
  { key: "feedback", label: "Add Feedback" },
  { key: "password", label: "Password Change" },
  { key: "details", label: "View details" },
  { key: "history", label: "Transaction History" },
];

export default function UserDashboard({ onExit }) {
  const [active, setActive] = useState(null);
  const [amount, setAmount] = useState("");
  const [toAccount, setToAccount] = useState("");
  const [loanPurpose, setLoanPurpose] = useState("");
  const [feedbackText, setFeedbackText] = useState("");
  const [oldPassword, setOldPassword] = useState("");
  const [newPassword, setNewPassword] = useState("");
  const [result, setResult] = useState(null);
  const [transactions, setTransactions] = useState(null);
  const [loans, setLoans] = useState(null);
  const [error, setError] = useState("");

  function reset() {
    setResult(null);
    setTransactions(null);
    setLoans(null);
    setError("");
  }

  async function run(action) {
    reset();
    try {
      switch (action) {
        case "deposit": {
          const r = await api.deposit(amount);
          setResult(`New balance: ${r.balance.toFixed(2)}`);
          break;
        }
        case "withdraw": {
          const r = await api.withdraw(amount);
          setResult(`New balance: ${r.balance.toFixed(2)}`);
          break;
        }
        case "transfer": {
          const r = await api.transfer(toAccount, amount);
          setResult(`Transfer sent. Your new balance: ${r.balance.toFixed(2)}`);
          break;
        }
        case "balance": {
          const r = await api.balance();
          setResult(`Balance: ${r.balance.toFixed(2)}`);
          break;
        }
        case "loan_apply": {
          const r = await api.loanApply(amount, loanPurpose);
          setResult(`Loan application submitted, id=${r.loan_id}`);
          setLoanPurpose("");
          break;
        }
        case "loan_list": {
          const r = await api.loanMyList();
          setLoans(r.loans);
          break;
        }
        case "feedback": {
          const r = await api.feedbackAdd(feedbackText);
          setResult(`Feedback submitted, id=${r.feedback_id}`);
          setFeedbackText("");
          break;
        }
        case "password": {
          const r = await api.changePassword(oldPassword, newPassword);
          setResult(r.message);
          setOldPassword("");
          setNewPassword("");
          break;
        }
        case "details": {
          const r = await api.viewDetails();
          setResult(JSON.stringify(r.details, null, 2));
          break;
        }
        case "history": {
          const r = await api.history();
          setTransactions(r.transactions);
          break;
        }
      }
    } catch (err) {
      setError(err.message);
    }
  }

  async function handleExit() {
    await api.logout().catch(() => {});
    onExit();
  }

  return (
    <div className="card">
      <h1>Do you want to:</h1>
      <div className="menu">
        {MENU.map((m) => (
          <button key={m.key} className={active === m.key ? "active" : ""} onClick={() => { setActive(m.key); reset(); }}>
            {m.label}
          </button>
        ))}
        <button onClick={handleExit}>Exit</button>
      </div>

      {(active === "deposit" || active === "withdraw") && (
        <div className="panel">
          <label>
            Amount
            <input value={amount} onChange={(e) => setAmount(e.target.value)} type="number" step="0.01" />
          </label>
          <button onClick={() => run(active)}>Submit</button>
        </div>
      )}

      {active === "transfer" && (
        <div className="panel">
          <label>
            To account ID
            <input value={toAccount} onChange={(e) => setToAccount(e.target.value)} inputMode="numeric" placeholder="e.g. 2" />
          </label>
          <label>
            Amount
            <input value={amount} onChange={(e) => setAmount(e.target.value)} type="number" step="0.01" />
          </label>
          <button onClick={() => run("transfer")}>Send transfer</button>
        </div>
      )}

      {active === "balance" && (
        <div className="panel">
          <button onClick={() => run("balance")}>Check balance</button>
        </div>
      )}

      {active === "loan_apply" && (
        <div className="panel">
          <label>
            Amount
            <input value={amount} onChange={(e) => setAmount(e.target.value)} type="number" step="0.01" />
          </label>
          <label>
            Purpose
            <input value={loanPurpose} onChange={(e) => setLoanPurpose(e.target.value)} placeholder="e.g. home renovation" />
          </label>
          <button onClick={() => run("loan_apply")}>Submit application</button>
        </div>
      )}

      {active === "loan_list" && (
        <div className="panel">
          <button onClick={() => run("loan_list")}>Refresh my loans</button>
          {loans && loans.length === 0 && <p>No loan applications yet.</p>}
          {loans && loans.length > 0 && (
            <div className="table-wrap">
              <table className="data-table">
                <thead>
                  <tr><th>ID</th><th>Amount</th><th>Purpose</th><th>Status</th></tr>
                </thead>
                <tbody>
                  {loans.map((l) => (
                    <tr key={l.id}>
                      <td>{l.id}</td>
                      <td>{l.amount}</td>
                      <td>{l.purpose}</td>
                      <td>{l.status}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </div>
      )}

      {active === "feedback" && (
        <div className="panel">
          <label>
            Feedback
            <input value={feedbackText} onChange={(e) => setFeedbackText(e.target.value)} placeholder="Tell us what you think" />
          </label>
          <button onClick={() => run("feedback")}>Submit feedback</button>
        </div>
      )}

      {active === "password" && (
        <div className="panel">
          <label>
            Old password
            <input type="password" value={oldPassword} onChange={(e) => setOldPassword(e.target.value)} />
          </label>
          <label>
            New password
            <input type="password" value={newPassword} onChange={(e) => setNewPassword(e.target.value)} />
          </label>
          <button onClick={() => run("password")}>Change password</button>
        </div>
      )}

      {active === "details" && (
        <div className="panel">
          <button onClick={() => run("details")}>Fetch details</button>
        </div>
      )}

      {active === "history" && (
        <div className="panel">
          <button onClick={() => run("history")}>Refresh history</button>
          {transactions && transactions.length === 0 && <p>No transactions yet.</p>}
          {transactions && transactions.length > 0 && (
            <div className="table-wrap">
              <table className="data-table">
                <thead>
                  <tr>
                    <th>Date &amp; time</th>
                    <th>By</th>
                    <th>Operation</th>
                    <th>Amount</th>
                    <th>Balance after</th>
                    <th>Counterparty</th>
                  </tr>
                </thead>
                <tbody>
                  {transactions.map((t, i) => (
                    <tr key={i}>
                      <td>{t.timestamp}</td>
                      <td>{t.owner}</td>
                      <td>{t.operation}</td>
                      <td>{t.amount}</td>
                      <td>{t.balance}</td>
                      <td>{t.counterparty}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </div>
      )}

      {error && <p className="error">{error}</p>}
      {result && <pre className="result">{result}</pre>}
    </div>
  );
}
