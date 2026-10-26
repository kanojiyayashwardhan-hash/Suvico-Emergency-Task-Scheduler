const $ = id => document.getElementById(id);
const locations = ["Control Center","Hospital","Fire Station","Market","Residential Area","Industrial Area"];
let lastSearch = null;
function toast(message,error=false){const el=$("toast");el.textContent=message;el.className=error?"show error":"show";setTimeout(()=>el.className="",2800)}
async function api(url,options={}){const r=await fetch(url,{headers:{"Content-Type":"application/json"},...options});const data=await r.json();if(!r.ok)throw new Error(data.error||"Request failed");return data}
function severityName(n){return ({4:"Critical",3:"High",2:"Medium",1:"Low"})[n]||"Low"}
function sevClass(n){return "s"+n}
function escapeHtml(s){return String(s??"").replace(/[&<>"']/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]))}
async function refresh(){
 try{
  const sort=$("sort").value;
  const [tasks,stats]=await Promise.all([api("/api/tasks?sort="+sort),api("/api/stats")]);
  for(const key of ["total","pending","inProgress","criticalPending"])$(key).textContent=stats[key];
  $("taskRows").innerHTML=tasks.length?tasks.map(t=>`<tr><td class="taskid">${escapeHtml(t.id)}</td><td>${escapeHtml(t.title)}</td><td>${escapeHtml(t.location)}</td><td><span class="severity ${sevClass(t.severity)}">${severityName(t.severity)}</span></td><td>${t.deadlineMinutes} min</td><td class="status">${escapeHtml(t.status)}</td><td>${t.status==="Pending"?`<button class="action" data-id="${t.id}" data-status="In Progress">Start</button> `:""}${t.status==="In Progress"?`<button class="action" data-id="${t.id}" data-status="Completed">Complete</button> `:""}${t.status==="Completed"||t.status==="Cancelled"?"":`<button class="action" data-id="${t.id}" data-status="Cancelled">Cancel</button>`}</td></tr>`).join(""):'<tr><td colspan="7" class="empty">No tasks found. Add your first task above.</td></tr>';
 }catch(e){toast(e.message,true)}
}
async function showNext(){
 try{const t=await api("/api/next");if(!t){$("nextTask").innerHTML='<div class="empty">No pending tasks yet.</div>';return}
 $("nextTask").innerHTML=`<span class="severity ${sevClass(t.severity)}">${severityName(t.severity)} PRIORITY</span><h3>${escapeHtml(t.title)}</h3><div class="next-meta"><span>${t.id}</span><span>• ${escapeHtml(t.location)}</span><span>• Deadline ${t.deadlineMinutes} min</span></div>`;
 }catch(e){toast(e.message,true)}
}
$("taskForm").addEventListener("submit",async e=>{e.preventDefault();try{const t=await api("/api/tasks",{method:"POST",body:JSON.stringify({title:$("title").value.trim(),location:$("location").value.trim()||"Unspecified",severity:Number($("severity").value),deadlineMinutes:Number($("deadline").value)})});$("taskForm").reset();$("severity").value="2";$("deadline").value="30";toast("Task "+t.id+" created.");await refresh();await showNext()}catch(e){toast(e.message,true)}});
$("taskRows").addEventListener("click",async e=>{const b=e.target.closest("button[data-id]");if(!b)return;try{await api("/api/tasks/"+b.dataset.id+"/status",{method:"POST",body:JSON.stringify({status:b.dataset.status})});toast(b.dataset.id+" marked "+b.dataset.status+".");await refresh();await showNext()}catch(err){toast(err.message,true)}});
$("sort").addEventListener("change",refresh);$("refreshBtn").addEventListener("click",async()=>{await refresh();await showNext();toast("Dashboard refreshed.")});
$("nextBtn").addEventListener("click",showNext);
$("searchBtn").addEventListener("click",async()=>{const id=$("searchId").value.trim();if(!id){await refresh();return}try{const t=await api("/api/tasks/"+encodeURIComponent(id));lastSearch=t;const rows=$("taskRows");rows.innerHTML=`<tr><td class="taskid">${escapeHtml(t.id)}</td><td>${escapeHtml(t.title)}</td><td>${escapeHtml(t.location)}</td><td><span class="severity ${sevClass(t.severity)}">${severityName(t.severity)}</span></td><td>${t.deadlineMinutes} min</td><td>${escapeHtml(t.status)}</td><td><button class="action" id="clearSearch">Show all</button></td></tr>`;toast("Found "+t.id)}catch(e){toast(e.message,true)}});
$("searchId").addEventListener("keydown",e=>{if(e.key==="Enter")$("searchBtn").click()});
$("taskRows").addEventListener("click",e=>{if(e.target.id==="clearSearch"){ $("searchId").value="";refresh()}});
$("undoBtn").addEventListener("click",async()=>{try{const t=await api("/api/undo",{method:"POST"});toast("Restored previous status for "+t.id);await refresh();await showNext()}catch(e){toast(e.message,true)}});
for(const id of ["routeFrom","routeTo"])$(id).innerHTML=locations.map(l=>`<option>${l}</option>`).join("");
$("routeTo").value="Hospital";
$("routeBtn").addEventListener("click",async()=>{try{const from=$("routeFrom").value,to=$("routeTo").value;const r=await api("/api/route?from="+encodeURIComponent(from)+"&to="+encodeURIComponent(to));$("routeResult").innerHTML=`<b>${r.estimatedMinutes} estimated minutes</b><div class="route-path">${r.path.map(escapeHtml).join(" → ")}</div><small>${escapeHtml(r.note)}</small>`}catch(e){toast(e.message,true)}});
refresh();showNext();
