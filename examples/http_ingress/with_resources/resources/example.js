setInterval(() => {
  fetch("requests.json")
  .then(response => response.json())
  .then(data => {
    document.getElementById('counter_countainer').innerHTML = data.request
  });
}, 1000);