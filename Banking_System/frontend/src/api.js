// Must use the same hostname the page itself is served from (localhost),
// not 127.0.0.1 -- browsers treat those as different sites, so a
// SameSite=Lax session cookie set by one would silently not be sent to
// the other. Overridable at build time via VITE_API_URL; the default
// assumes the gateway is available on localhost:8077.
const BASE_URL = import.meta.env.VITE_API_URL || "http://localhost:8077";
const SESSION_TOKEN_KEY = "banking_session_token";

let sessionToken = sessionStorage.getItem(SESSION_TOKEN_KEY);

function requestHeaders(extraHeaders = {}) {
  return {
    "Content-Type": "application/json",
    ...(sessionToken ? { "X-Session-Token": sessionToken } : {}),
    ...extraHeaders,
  };
}

function clearExpiredSession() {
  sessionToken = null;
  sessionStorage.removeItem(SESSION_TOKEN_KEY);
  sessionStorage.removeItem("banking_role");
  window.dispatchEvent(new Event("banking-session-expired"));
}

// FastAPI returns two different shapes under `detail`: a plain string for
// errors we raise ourselves (HTTPException), or an array of per-field
// validation-error objects ({loc, msg, type, ...}) when a request body
// fails pydantic validation before our code even runs (e.g. a non-numeric
// account ID). Passing the array straight to `new Error()` stringifies it
// as "[object Object]" -- this turns either shape into one readable line.
function formatErrorDetail(detail) {
  if (!detail) return null;
  if (typeof detail === "string") return detail;
  if (Array.isArray(detail)) {
    return detail
      .map((e) => {
        const field = Array.isArray(e.loc) ? e.loc[e.loc.length - 1] : "field";
        return `${field}: ${e.msg || "invalid value"}`;
      })
      .join("; ");
  }
  return JSON.stringify(detail);
}

async function call(path, options = {}) {
  const res = await fetch(BASE_URL + path, {
    credentials: "include",
    ...options,
    headers: requestHeaders(options.headers),
  });
  const body = await res.json().catch(() => ({}));
  if (!res.ok) {
    if (res.status === 401 && path !== "/api/login" && path !== "/api/logout" && sessionToken) {
      clearExpiredSession();
    }
    throw new Error(formatErrorDetail(body.detail) || `request failed (${res.status})`);
  }
  return body;
}

export const api = {
  setSessionToken: (token) => {
    sessionToken = token || null;
    if (sessionToken) sessionStorage.setItem(SESSION_TOKEN_KEY, sessionToken);
    else sessionStorage.removeItem(SESSION_TOKEN_KEY);
  },
  hasSessionToken: () => Boolean(sessionToken),
  login: (kind, id, password) => {
    const numericId = Number(id);
    if (!Number.isInteger(numericId)) {
      throw new Error(`${kind === "ADMIN" ? "Admin ID" : kind === "STAFF" ? "Staff ID" : "Account ID"} must be a number, e.g. 1`);
    }
    return call("/api/login", { method: "POST", body: JSON.stringify({ kind, id: numericId, password }) });
  },
  logout: () => call("/api/logout", { method: "POST" }),
  deposit: (amount) => call("/api/deposit", { method: "POST", body: JSON.stringify({ amount: Number(amount) }) }),
  withdraw: (amount) => call("/api/withdraw", { method: "POST", body: JSON.stringify({ amount: Number(amount) }) }),
  transfer: (toAccountId, amount) =>
    call("/api/transfer", { method: "POST", body: JSON.stringify({ to_account_id: Number(toAccountId), amount: Number(amount) }) }),
  balance: () => call("/api/balance"),
  changePassword: (oldPassword, newPassword) =>
    call("/api/password", {
      method: "POST",
      body: JSON.stringify({ old_password: oldPassword, new_password: newPassword }),
    }),
  viewDetails: () => call("/api/details"),
  history: () => call("/api/history"),

  loanApply: (amount, purpose) =>
    call("/api/loans", { method: "POST", body: JSON.stringify({ amount: Number(amount), purpose }) }),
  loanMyList: () => call("/api/loans/mine"),
  loanAssignedList: () => call("/api/loans/assigned"),
  loanPendingList: () => call("/api/loans/pending"),
  loanDecisionHistory: () => call("/api/loans/decisions"),
  loanDecide: (loanId, decision) =>
    call(`/api/loans/${loanId}/decision`, { method: "POST", body: JSON.stringify({ decision }) }),
  loanAssign: (loanId, employeeId) =>
    call(`/api/loans/${loanId}/assign`, { method: "POST", body: JSON.stringify({ employee_id: Number(employeeId) }) }),

  feedbackAdd: (text) => call("/api/feedback", { method: "POST", body: JSON.stringify({ text }) }),
  feedbackList: () => call("/api/feedback"),

  accountSetStatus: (accountId, status) =>
    call(`/api/manager/accounts/${accountId}/status`, { method: "PUT", body: JSON.stringify({ status }) }),

  staffAdd: (role, name, password) =>
    call("/api/staff", { method: "POST", body: JSON.stringify({ role, name, password }) }),
  staffModify: (role, staffId, field, value) =>
    call(`/api/staff/${role}/${staffId}`, { method: "PUT", body: JSON.stringify({ field, value }) }),
  staffList: () => call("/api/staff"),

  adminAdd: (payload) => call("/api/admin/accounts", { method: "POST", body: JSON.stringify(payload) }),
  adminDelete: (accountId) => call(`/api/admin/accounts/${accountId}`, { method: "DELETE" }),
  closeCustomer: (accountId) => call(`/api/customers/${accountId}`, { method: "DELETE" }),
  adminModify: (accountId, field, value) =>
    call(`/api/admin/accounts/${accountId}`, { method: "PUT", body: JSON.stringify({ field, value }) }),
  adminSearch: (accountId) => call(`/api/admin/accounts/${accountId}`),
  adminList: () => call("/api/admin/accounts"),
  adminHistory: (accountId) => call(`/api/admin/accounts/${accountId}/history`),
};
