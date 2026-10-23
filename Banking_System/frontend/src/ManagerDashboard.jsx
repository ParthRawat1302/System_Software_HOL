import { useState } from "react";
import { api } from "./api";

export default function ManagerDashboard({ onExit }) {
  const [active, setActive] = useState(null);
  const [result, setResult] = useState(null);
  const [error, setError] = useState("");
  const [feedback, setFeedback] = useState(null);
  const [staff, setStaff] = useState(null);
  const [pendingLoans, setPendingLoans] = useState(null);
  const [loanHistory, setLoanHistory] = useState(null);

  const [accountId, setAccountId] = useState("");
  const [status, setStatus] = useState("INACTIVE");
  const [loanId, setLoanId] = useState("");
  const [employeeId, setEmployeeId] = useState("");
  const [oldPassword, setOldPassword] = useState("");
  const [newPassword, setNewPassword] = useState("");

  function reset() {
    setResult(null);
    setError("");
  }

  function select(tab) {
    setActive(tab);
    reset();
    setFeedback(null);
    setStaff(null);
    setPendingLoans(null);
    setLoanHistory(null);
  }

  async function run(action) {
    reset();
    try {
      switch (action) {
        case "status": {
          const r = await api.accountSetStatus(accountId, status);
          setResult(r.message);
          break;
        }
        case "assign": {
          const r = await api.loanAssign(loanId, employeeId);
          setResult(r.message);
          break;
        }
        case "pendingLoans": {
          const r = await api.loanPendingList();
          setPendingLoans(r.loans);
          break;
        }
        case "loanHistory": {
          const r = await api.loanDecisionHistory();
          setLoanHistory(r.loans);
          break;
        }
        case "feedback": {
          const r = await api.feedbackList();
          setFeedback(r.feedback);
          break;
        }
        case "staff": {
          const r = await api.staffList();
          setStaff(r.staff);
          break;
        }
        case "password": {
          const r = await api.changePassword(oldPassword, newPassword);
          setResult(r.message);
          setOldPassword("");
          setNewPassword("");
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
      <h1>Manager</h1>
      <div className="menu">
        <button className={active === "status" ? "active" : ""} onClick={() => select("status")}>Activate/Deactivate Account</button>
        <button className={active === "assign" ? "active" : ""} onClick={() => select("assign")}>Assign Loan to Employee</button>
        <button className={active === "pendingLoans" ? "active" : ""} onClick={() => select("pendingLoans")}>Pending Loan Requests</button>
        <button className={active === "loanHistory" ? "active" : ""} onClick={() => select("loanHistory")}>Loan Decision History</button>
        <button className={active === "feedback" ? "active" : ""} onClick={() => select("feedback")}>Review Feedback</button>
        <button className={active === "staff" ? "active" : ""} onClick={() => select("staff")}>Staff List</button>
        <button className={active === "password" ? "active" : ""} onClick={() => select("password")}>Change Password</button>
        <button onClick={handleExit}>Exit</button>
      </div>

      {active === "status" && (
        <div className="panel">
          <label>
            Account ID
            <input value={accountId} onChange={(e) => setAccountId(e.target.value)} />
          </label>
          <label>
            New status
            <select value={status} onChange={(e) => setStatus(e.target.value)}>
              <option value="ACTIVE">Activate</option>
              <option value="INACTIVE">Deactivate</option>
            </select>
          </label>
          <button onClick={() => run("status")}>Apply</button>
        </div>
      )}

      {active === "assign" && (
        <div className="panel">
          <label>
            Loan ID
            <input value={loanId} onChange={(e) => setLoanId(e.target.value)} />
          </label>
          <label>
            Employee ID
            <input value={employeeId} onChange={(e) => setEmployeeId(e.target.value)} />
          </label>
          <button onClick={() => run("assign")}>Assign</button>
          <p style={{ fontSize: "0.8rem", color: "#666" }}>Tip: check the Staff List tab for employee IDs.</p>
        </div>
      )}

      {active === "pendingLoans" && (
        <div className="panel">
          <button onClick={() => run("pendingLoans")}>Refresh pending requests</button>
          {pendingLoans && pendingLoans.length === 0 && <p>No unassigned loan requests.</p>}
          {pendingLoans && pendingLoans.length > 0 && (
            <div className="table-wrap">
              <table className="data-table">
                <thead><tr><th>Loan ID</th><th>Account</th><th>Amount</th><th>Purpose</th></tr></thead>
                <tbody>
                  {pendingLoans.map((loan) => (
                    <tr key={loan.id}><td>{loan.id}</td><td>{loan.account}</td><td>{loan.amount}</td><td>{loan.purpose}</td></tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </div>
      )}

      {active === "loanHistory" && (
        <div className="panel">
          <button onClick={() => run("loanHistory")}>Refresh loan history</button>
          {loanHistory && loanHistory.length === 0 && <p>No loan requests yet.</p>}
          {loanHistory && loanHistory.length > 0 && (
            <div className="table-wrap"><table className="data-table">
              <thead><tr><th>Loan</th><th>Account</th><th>Amount</th><th>Purpose</th><th>Status</th><th>Assigned employee</th><th>Decision by</th></tr></thead>
              <tbody>{loanHistory.map((loan) => <tr key={loan.id}><td>{loan.id}</td><td>{loan.account}</td><td>{loan.amount}</td><td>{loan.purpose}</td><td>{loan.status}</td><td>{loan.assigned}</td><td>{loan.decided_by}</td></tr>)}</tbody>
            </table></div>
          )}
        </div>
      )}

      {active === "feedback" && (
        <div className="panel">
          <button onClick={() => run("feedback")}>Refresh feedback</button>
          {feedback && feedback.length === 0 && <p>No feedback yet.</p>}
          {feedback && feedback.length > 0 && (
            <div className="table-wrap">
              <table className="data-table">
                <thead><tr><th>ID</th><th>Account</th><th>Text</th></tr></thead>
                <tbody>
                  {feedback.map((f) => (
                    <tr key={f.id}><td>{f.id}</td><td>{f.account}</td><td>{f.text}</td></tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </div>
      )}

      {active === "staff" && (
        <div className="panel">
          <button onClick={() => run("staff")}>Refresh staff list</button>
          {staff && staff.length === 0 && <p>No staff members yet.</p>}
          {staff && staff.length > 0 && (
            <div className="table-wrap">
              <table className="data-table">
                <thead><tr><th>ID</th><th>Role</th><th>Status</th><th>Name</th></tr></thead>
                <tbody>
                  {staff.map((s) => (
                    <tr key={s.id}><td>{s.id}</td><td>{s.role}</td><td>{s.status}</td><td>{s.name}</td></tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
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

      {error && <p className="error">{error}</p>}
      {result && <pre className="result">{result}</pre>}
    </div>
  );
}
