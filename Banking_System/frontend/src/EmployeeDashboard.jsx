import { useState } from "react";
import { api } from "./api";

export default function EmployeeDashboard({ onExit }) {
  const [active, setActive] = useState(null);
  const [result, setResult] = useState(null);
  const [error, setError] = useState("");
  const [accounts, setAccounts] = useState(null);
  const [loans, setLoans] = useState(null);
  const [loanHistory, setLoanHistory] = useState(null);
  const [transactions, setTransactions] = useState(null);

  const [addForm, setAddForm] = useState({
    role: "NORMAL",
    owner1_name: "",
    owner1_password: "",
    owner2_name: "-",
    owner2_password: "-",
    initial_balance: "0",
  });
  const [targetId, setTargetId] = useState("");
  const [modifyField, setModifyField] = useState("balance");
  const [modifyValue, setModifyValue] = useState("");
  const [oldPassword, setOldPassword] = useState("");
  const [newPassword, setNewPassword] = useState("");

  function reset() {
    setResult(null);
    setTransactions(null);
    setError("");
  }

  async function run(action) {
    reset();
    const operation = typeof action === "string" ? action : "decide";
    try {
      switch (operation) {
        case "add": {
          const r = await api.adminAdd({ ...addForm, initial_balance: Number(addForm.initial_balance) });
          setResult(`Created account id=${r.account_id}`);
          break;
        }
        case "modify": {
          const r = await api.adminModify(targetId, modifyField, modifyValue);
          setResult(r.message);
          break;
        }
        case "close": {
          const r = await api.closeCustomer(targetId);
          setResult(`${r.message}. Customer transaction history has been retained.`);
          break;
        }
        case "search": {
          const r = await api.adminSearch(targetId);
          setResult(JSON.stringify(r.details, null, 2));
          break;
        }
        case "list": {
          const r = await api.adminList();
          setAccounts(r.accounts);
          break;
        }
        case "history": {
          const r = await api.adminHistory(targetId);
          setTransactions(r.transactions);
          break;
        }
        case "assigned": {
          const r = await api.loanAssignedList();
          setLoans(r.loans);
          break;
        }
        case "loanHistory": {
          const r = await api.loanDecisionHistory();
          setLoanHistory(r.loans);
          break;
        }
        case "decide": {
          const r = await api.loanDecide(action.loanId, action.decision);
          setResult(r.message);
          setLoans((current) => current?.filter((loan) => loan.id !== String(action.loanId)) ?? current);
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

  function select(tab) {
    setActive(tab);
    reset();
    setAccounts(null);
    setLoans(null);
    setLoanHistory(null);
  }

  return (
    <div className="card">
      <h1>Bank Employee</h1>
      <div className="menu">
        <button className={active === "add" ? "active" : ""} onClick={() => select("add")}>Add Customer</button>
        <button className={active === "modify" ? "active" : ""} onClick={() => select("modify")}>Modify Customer</button>
        <button className={active === "close" ? "active" : ""} onClick={() => select("close")}>Close Customer Account</button>
        <button className={active === "search" ? "active" : ""} onClick={() => select("search")}>Search Customer</button>
        <button className={active === "list" ? "active" : ""} onClick={() => select("list")}>List Customers</button>
        <button className={active === "history" ? "active" : ""} onClick={() => select("history")}>Customer Passbook</button>
        <button className={active === "assigned" ? "active" : ""} onClick={() => select("assigned")}>Assigned Loans</button>
        <button className={active === "loanHistory" ? "active" : ""} onClick={() => select("loanHistory")}>Loan Decision History</button>
        <button className={active === "password" ? "active" : ""} onClick={() => select("password")}>Change Password</button>
        <button onClick={handleExit}>Exit</button>
      </div>

      {active === "add" && (
        <div className="panel">
          <label>
            Role
            <select value={addForm.role} onChange={(e) => setAddForm({ ...addForm, role: e.target.value })}>
              <option value="NORMAL">Normal</option>
              <option value="JOINT">Joint</option>
            </select>
          </label>
          <label>
            Owner 1 name
            <input value={addForm.owner1_name} onChange={(e) => setAddForm({ ...addForm, owner1_name: e.target.value })} />
          </label>
          <label>
            Owner 1 password
            <input value={addForm.owner1_password} onChange={(e) => setAddForm({ ...addForm, owner1_password: e.target.value })} />
          </label>
          {addForm.role === "JOINT" && (
            <>
              <label>
                Owner 2 name
                <input value={addForm.owner2_name} onChange={(e) => setAddForm({ ...addForm, owner2_name: e.target.value })} />
              </label>
              <label>
                Owner 2 password
                <input value={addForm.owner2_password} onChange={(e) => setAddForm({ ...addForm, owner2_password: e.target.value })} />
              </label>
            </>
          )}
          <label>
            Initial balance
            <input type="number" step="0.01" value={addForm.initial_balance} onChange={(e) => setAddForm({ ...addForm, initial_balance: e.target.value })} />
          </label>
          <button onClick={() => run("add")}>Create</button>
        </div>
      )}

      {(active === "modify" || active === "close" || active === "search" || active === "history") && (
        <div className="panel">
          <label>
            Account ID
            <input value={targetId} onChange={(e) => setTargetId(e.target.value)} />
          </label>
          {active === "modify" && (
            <>
              <label>
                Field
                <select value={modifyField} onChange={(e) => setModifyField(e.target.value)}>
                  <option value="balance">balance</option>
                  <option value="owner1_name">owner1_name</option>
                  <option value="owner2_name">owner2_name</option>
                </select>
              </label>
              <label>
                Value
                <input value={modifyValue} onChange={(e) => setModifyValue(e.target.value)} />
              </label>
            </>
          )}
          <button onClick={() => run(active)}>
            {active === "modify" ? "Modify" : active === "close" ? "Close account" : active === "search" ? "Search" : "View passbook"}
          </button>
        </div>
      )}

      {active === "list" && (
        <div className="panel">
          <button onClick={() => run("list")}>Refresh list</button>
          {accounts && accounts.length === 0 && <p>No active accounts.</p>}
          {accounts && accounts.length > 0 && (
            <div className="table-wrap">
              <table className="data-table">
                <thead><tr><th>ID</th><th>Role</th><th>Status</th><th>Owner 1</th><th>Owner 2</th><th>Balance</th></tr></thead>
                <tbody>
                  {accounts.map((a) => (
                    <tr key={a.id}>
                      <td>{a.id}</td><td>{a.role}</td><td>{a.status}</td><td>{a.owner1}</td><td>{a.owner2 || "-"}</td><td>{a.balance}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </div>
      )}

      {active === "assigned" && (
        <div className="panel">
          <button onClick={() => run("assigned")}>Refresh loans awaiting a decision</button>
          {loans && loans.length === 0 && <p>No loans assigned to you.</p>}
          {loans && loans.length > 0 && (
            <div className="table-wrap">
              <table className="data-table">
                <thead><tr><th>ID</th><th>Account</th><th>Amount</th><th>Purpose</th><th>Status</th><th>Decision</th></tr></thead>
                <tbody>
                  {loans.map((l) => (
                    <tr key={l.id}>
                      <td>{l.id}</td><td>{l.account}</td><td>{l.amount}</td><td>{l.purpose}</td><td>{l.status}</td>
                      <td>
                        <button onClick={() => run({ loanId: l.id, decision: "APPROVE" })}>Approve</button>
                        <button onClick={() => run({ loanId: l.id, decision: "REJECT" })}>Reject</button>
                      </td>
                    </tr>
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
            <div className="table-wrap">
              <table className="data-table">
                <thead><tr><th>Loan</th><th>Account</th><th>Amount</th><th>Purpose</th><th>Status</th><th>Assigned employee</th><th>Decision by</th></tr></thead>
                <tbody>{loanHistory.map((loan) => (
                  <tr key={loan.id}><td>{loan.id}</td><td>{loan.account}</td><td>{loan.amount}</td><td>{loan.purpose}</td><td>{loan.status}</td><td>{loan.assigned}</td><td>{loan.decided_by}</td></tr>
                ))}</tbody>
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

      {active === "history" && transactions && transactions.length === 0 && (
        <p>No transactions found for this account.</p>
      )}
      {active === "history" && transactions && transactions.length > 0 && (
        <div className="table-wrap">
          <table className="data-table">
            <thead>
              <tr>
                <th>Date &amp; time</th>
                <th>Account ID</th>
                <th>By</th>
                <th>Operation</th>
                <th>Amount</th>
                <th>Balance after</th>
                <th>Counterparty</th>
              </tr>
            </thead>
            <tbody>
              {transactions.map((transaction, index) => (
                <tr key={`${transaction.timestamp}-${index}`}>
                  <td>{transaction.timestamp}</td>
                  <td>{transaction.account_id}</td>
                  <td>{transaction.owner}</td>
                  <td>{transaction.operation}</td>
                  <td>{transaction.amount}</td>
                  <td>{transaction.balance}</td>
                  <td>{transaction.counterparty}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}

      {error && <p className="error">{error}</p>}
      {result && <pre className="result">{result}</pre>}
    </div>
  );
}
