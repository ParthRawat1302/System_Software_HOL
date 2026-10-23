import { useState } from "react";
import { api } from "./api";

export default function AdminDashboard({ onExit }) {
  const [active, setActive] = useState(null);
  const [result, setResult] = useState(null);
  const [error, setError] = useState("");

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
  const [accounts, setAccounts] = useState(null);
  const [transactions, setTransactions] = useState(null);
  const [staff, setStaff] = useState(null);
  const [staffForm, setStaffForm] = useState({ role: "EMPLOYEE", name: "", password: "" });
  const [staffId, setStaffId] = useState("");
  const [staffTargetRole, setStaffTargetRole] = useState("EMPLOYEE");
  const [staffField, setStaffField] = useState("role");
  const [staffValue, setStaffValue] = useState("");
  const [oldPassword, setOldPassword] = useState("");
  const [newPassword, setNewPassword] = useState("");
  const [loans, setLoans] = useState(null);
  const [feedback, setFeedback] = useState(null);
  const [loanId, setLoanId] = useState("");
  const [employeeId, setEmployeeId] = useState("");
  const [accountStatus, setAccountStatus] = useState("INACTIVE");

  function reset() {
    setResult(null);
    setError("");
  }

  async function handleAdd() {
    reset();
    try {
      const r = await api.adminAdd({ ...addForm, initial_balance: Number(addForm.initial_balance) });
      setResult(`Created account id=${r.account_id}`);
    } catch (err) {
      setError(err.message);
    }
  }

  async function handleDelete() {
    reset();
    try {
      const r = await api.adminDelete(targetId);
      setResult(r.message);
    } catch (err) {
      setError(err.message);
    }
  }

  async function handleModify() {
    reset();
    try {
      const r = await api.adminModify(targetId, modifyField, modifyValue);
      setResult(r.message);
    } catch (err) {
      setError(err.message);
    }
  }

  async function handleSearch() {
    reset();
    try {
      const r = await api.adminSearch(targetId);
      setResult(JSON.stringify(r.details, null, 2));
    } catch (err) {
      setError(err.message);
    }
  }

  async function handleHistory() {
    reset();
    try {
      const r = await api.adminHistory(targetId);
      setTransactions(r.transactions);
    } catch (err) {
      setError(err.message);
    }
  }

  async function handleList() {
    reset();
    try {
      const r = await api.adminList();
      setAccounts(r.accounts);
    } catch (err) {
      setError(err.message);
    }
  }

  async function handleStaffAdd() {
    reset();
    try {
      const r = await api.staffAdd(staffForm.role, staffForm.name, staffForm.password);
      setResult(`Created ${staffForm.role.toLowerCase()} id=${r.staff_id}`);
    } catch (err) {
      setError(err.message);
    }
  }

  async function handleStaffModify() {
    reset();
    try {
      const r = await api.staffModify(staffTargetRole, staffId, staffField, staffValue);
      setResult(r.message);
    } catch (err) {
      setError(err.message);
    }
  }

  async function handleStaffList() {
    reset();
    try {
      const r = await api.staffList();
      setStaff(r.staff);
    } catch (err) {
      setError(err.message);
    }
  }

  async function handlePasswordChange() {
    reset();
    try {
      const r = await api.changePassword(oldPassword, newPassword);
      setResult(r.message);
      setOldPassword("");
      setNewPassword("");
    } catch (err) {
      setError(err.message);
    }
  }

  async function loadLoans(pendingOnly = false) {
    reset();
    try {
      const r = pendingOnly ? await api.loanPendingList() : await api.loanDecisionHistory();
      setLoans(r.loans);
    } catch (err) { setError(err.message); }
  }

  async function handleAssignLoan(id = loanId) {
    reset();
    try {
      const r = await api.loanAssign(id, employeeId);
      setResult(r.message);
      await loadLoans(true);
    } catch (err) { setError(err.message); }
  }

  async function handleLoanDecision(id, decision) {
    reset();
    try {
      const r = await api.loanDecide(id, decision);
      setResult(r.message);
      await loadLoans(false);
    } catch (err) { setError(err.message); }
  }

  async function handleFeedbackList() {
    reset();
    try { const r = await api.feedbackList(); setFeedback(r.feedback); }
    catch (err) { setError(err.message); }
  }

  async function handleAccountStatus() {
    reset();
    try {
      const r = await api.accountSetStatus(targetId, accountStatus);
      setResult(r.message);
    } catch (err) { setError(err.message); }
  }

  async function handleExit() {
    await api.logout().catch(() => {});
    onExit();
  }

  return (
    <div className="card">
      <h1>Administrator</h1>
      <div className="menu">
        <button className={active === "add" ? "active" : ""} onClick={() => { setActive("add"); reset(); }}>Add account</button>
        <button className={active === "delete" ? "active" : ""} onClick={() => { setActive("delete"); reset(); }}>Delete account</button>
        <button className={active === "modify" ? "active" : ""} onClick={() => { setActive("modify"); reset(); }}>Modify account</button>
        <button className={active === "search" ? "active" : ""} onClick={() => { setActive("search"); reset(); }}>Search account</button>
        <button className={active === "history" ? "active" : ""} onClick={() => { setActive("history"); reset(); setTransactions(null); }}>Account history</button>
        <button className={active === "list" ? "active" : ""} onClick={() => { setActive("list"); reset(); setAccounts(null); }}>List accounts</button>
        <button className={active === "staff_add" ? "active" : ""} onClick={() => { setActive("staff_add"); reset(); }}>Add Staff</button>
        <button className={active === "staff_modify" ? "active" : ""} onClick={() => { setActive("staff_modify"); reset(); }}>Modify Staff</button>
        <button className={active === "staff_list" ? "active" : ""} onClick={() => { setActive("staff_list"); reset(); setStaff(null); }}>List Staff</button>
        <button className={active === "account_status" ? "active" : ""} onClick={() => { setActive("account_status"); reset(); }}>Activate/Deactivate Account</button>
        <button className={active === "loans" ? "active" : ""} onClick={() => { setActive("loans"); reset(); setLoans(null); }}>Loan Requests &amp; Decisions</button>
        <button className={active === "feedback" ? "active" : ""} onClick={() => { setActive("feedback"); reset(); setFeedback(null); }}>Review Feedback</button>
        <button className={active === "password" ? "active" : ""} onClick={() => { setActive("password"); reset(); }}>Change Password</button>
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
          <button onClick={handleAdd}>Create</button>
        </div>
      )}

      {(active === "delete" || active === "modify" || active === "search" || active === "history") && (
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
          <button
            onClick={
              active === "delete" ? handleDelete
              : active === "modify" ? handleModify
              : active === "history" ? handleHistory
              : handleSearch
            }
          >
            {active === "delete" ? "Delete" : active === "modify" ? "Modify" : active === "history" ? "View history" : "Search"}
          </button>

          {active === "history" && transactions && transactions.length === 0 && <p>No transactions yet.</p>}
          {active === "history" && transactions && transactions.length > 0 && (
            <div className="table-wrap">
              <table className="data-table">
                <thead>
                  <tr>
                    <th>Date &amp; time</th>
                    <th>By</th>
                    <th>Operation</th>
                    <th>Amount</th>
                    <th>Balance after</th>
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
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </div>
      )}

      {active === "list" && (
        <div className="panel">
          <button onClick={handleList}>Refresh list</button>
          {accounts && accounts.length === 0 && <p>No active accounts.</p>}
          {accounts && accounts.length > 0 && (
            <table className="data-table">
              <thead>
                <tr>
                  <th>ID</th>
                  <th>Role</th>
                  <th>Status</th>
                  <th>Owner 1</th>
                  <th>Owner 2</th>
                  <th>Balance</th>
                </tr>
              </thead>
              <tbody>
                {accounts.map((a) => (
                  <tr key={a.id}>
                    <td>{a.id}</td>
                    <td>{a.role}</td>
                    <td>{a.status}</td>
                    <td>{a.owner1}</td>
                    <td>{a.owner2 || "-"}</td>
                    <td>{a.balance}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>
      )}

      {active === "staff_add" && (
        <div className="panel">
          <label>
            Role
            <select value={staffForm.role} onChange={(e) => setStaffForm({ ...staffForm, role: e.target.value })}>
              <option value="EMPLOYEE">Employee</option>
              <option value="MANAGER">Manager</option>
            </select>
          </label>
          <label>
            Name
            <input value={staffForm.name} onChange={(e) => setStaffForm({ ...staffForm, name: e.target.value })} />
          </label>
          <label>
            Password
            <input value={staffForm.password} onChange={(e) => setStaffForm({ ...staffForm, password: e.target.value })} />
          </label>
          <button onClick={handleStaffAdd}>Create staff member</button>
        </div>
      )}

      {active === "staff_modify" && (
        <div className="panel">
          <label>
            Staff type
            <select value={staffTargetRole} onChange={(e) => setStaffTargetRole(e.target.value)}>
              <option value="EMPLOYEE">Employee</option>
              <option value="MANAGER">Manager</option>
            </select>
          </label>
          <label>
            Staff ID
            <input value={staffId} onChange={(e) => setStaffId(e.target.value)} />
          </label>
          <label>
            Field
            <select value={staffField} onChange={(e) => setStaffField(e.target.value)}>
              <option value="role">role (EMPLOYEE/MANAGER)</option>
              <option value="name">name</option>
              <option value="status">status (ACTIVE/DELETED)</option>
            </select>
          </label>
          <label>
            Value
            <input value={staffValue} onChange={(e) => setStaffValue(e.target.value)} />
          </label>
          <button onClick={handleStaffModify}>Modify</button>
        </div>
      )}

      {active === "staff_list" && (
        <div className="panel">
          <button onClick={handleStaffList}>Refresh staff list</button>
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

      {active === "account_status" && (
        <div className="panel">
          <label>Account ID<input value={targetId} onChange={(e) => setTargetId(e.target.value)} /></label>
          <label>New status
            <select value={accountStatus} onChange={(e) => setAccountStatus(e.target.value)}>
              <option value="ACTIVE">Activate</option>
              <option value="INACTIVE">Deactivate</option>
            </select>
          </label>
          <button onClick={handleAccountStatus}>Apply</button>
        </div>
      )}

      {active === "loans" && (
        <div className="panel">
          <button onClick={() => loadLoans(true)}>Refresh pending requests</button>
          {loans && loans.filter((loan) => loan.status === "PENDING").map((loan) => (
            <div key={loan.id} className="loan-row">
              Loan {loan.id}, account {loan.account}, {loan.amount}, {loan.purpose}
              <label>Employee ID<input value={employeeId} onChange={(e) => setEmployeeId(e.target.value)} /></label>
              <button onClick={() => handleAssignLoan(loan.id)}>Assign</button>
            </div>
          ))}
          <button onClick={() => loadLoans(false)}>Refresh decision history</button>
          {loans && loans.length > 0 && <div className="table-wrap"><table className="data-table">
            <thead><tr><th>Loan</th><th>Account</th><th>Amount</th><th>Purpose</th><th>Status</th><th>Employee</th><th>Decision by</th><th>Actions</th></tr></thead>
            <tbody>{loans.map((loan) => <tr key={loan.id}><td>{loan.id}</td><td>{loan.account}</td><td>{loan.amount}</td><td>{loan.purpose}</td><td>{loan.status}</td><td>{loan.assigned}</td><td>{loan.decided_by ?? "-"}</td><td>{loan.status === "PENDING" && loan.assigned !== "0" ? <><button onClick={() => handleLoanDecision(loan.id, "APPROVE")}>Approve</button><button onClick={() => handleLoanDecision(loan.id, "REJECT")}>Reject</button></> : "—"}</td></tr>)}</tbody>
          </table></div>}
        </div>
      )}

      {active === "feedback" && (
        <div className="panel">
          <button onClick={handleFeedbackList}>Refresh feedback</button>
          {feedback && feedback.length === 0 && <p>No feedback yet.</p>}
          {feedback && <div className="table-wrap"><table className="data-table"><thead><tr><th>ID</th><th>Account</th><th>Text</th></tr></thead><tbody>{feedback.map((item) => <tr key={item.id}><td>{item.id}</td><td>{item.account}</td><td>{item.text}</td></tr>)}</tbody></table></div>}
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
          <button onClick={handlePasswordChange}>Change password</button>
        </div>
      )}

      {error && <p className="error">{error}</p>}
      {result && <pre className="result">{result}</pre>}
    </div>
  );
}
