import { useEffect, useState } from "react";
import LoginPage from "./LoginPage";
import UserDashboard from "./UserDashboard";
import EmployeeDashboard from "./EmployeeDashboard";
import ManagerDashboard from "./ManagerDashboard";
import AdminDashboard from "./AdminDashboard";
import { api } from "./api";

const ROLE_STORAGE_KEY = "banking_role";
const VALID_ROLES = ["USER", "EMPLOYEE", "MANAGER", "ADMIN"];

export default function App() {
  // null | "USER" | "EMPLOYEE" | "MANAGER" | "ADMIN"
  const [role, setRole] = useState(() => {
    const savedRole = sessionStorage.getItem(ROLE_STORAGE_KEY);
    return VALID_ROLES.includes(savedRole) && api.hasSessionToken() ? savedRole : null;
  });

  useEffect(() => {
    function handleSessionExpired() {
      setRole(null);
    }
    window.addEventListener("banking-session-expired", handleSessionExpired);
    return () => window.removeEventListener("banking-session-expired", handleSessionExpired);
  }, []);

  function handleLoggedIn(kind, staffRole) {
    const nextRole = kind === "STAFF" ? staffRole : kind;
    if (VALID_ROLES.includes(nextRole)) {
      sessionStorage.setItem(ROLE_STORAGE_KEY, nextRole);
      setRole(nextRole);
    }
  }

  function handleExit() {
    sessionStorage.removeItem(ROLE_STORAGE_KEY);
    api.setSessionToken(null);
    setRole(null);
  }

  if (!role) return <LoginPage onLoggedIn={handleLoggedIn} />;
  if (role === "ADMIN") return <AdminDashboard onExit={handleExit} />;
  if (role === "MANAGER") return <ManagerDashboard onExit={handleExit} />;
  if (role === "EMPLOYEE") return <EmployeeDashboard onExit={handleExit} />;
  return <UserDashboard onExit={handleExit} />;
}
