// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var selectedApps = /** @dict */ {};

function generateContents(appIcon, appTitle, appPackageName) {
  var doc = document;
  var recommendAppsContainer = doc.getElementById('recommend-apps-container');
  var item = doc.createElement('div');
  item.classList.add('item');

  var checkbox = doc.createElement('input');
  checkbox.type = 'checkbox';
  checkbox.name = 'selectedApps';
  checkbox.setAttribute('data-title', appTitle);
  checkbox.setAttribute('data-packagename', appPackageName);
  checkbox.id = 'id_' + encodeURIComponent(appPackageName);
  checkbox.addEventListener('click', updateSelectedApps);

  var label = doc.createElement('label');
  label.htmlFor = 'id_' + encodeURIComponent(appPackageName);

  var img = doc.createElement('img');
  img.classList.add('app-icon');
  img.setAttribute('src', appIcon);

  var title = doc.createElement('span');
  title.classList.add('app-title');
  title.innerHTML = appTitle;

  label.appendChild(img);
  label.appendChild(title);

  item.appendChild(checkbox);
  item.appendChild(label);

  recommendAppsContainer.appendChild(item);
}

function updateSelectedApps(e) {
  var checkbox = e.target;
  if (checkbox.checked) {
    if (!(selectedApps.hasOwnProperty(
            checkbox.dataset.packagename))) {  // App is not selected before
      selectedApps[checkbox.dataset.packagename] = checkbox.dataset.title;
    }
  } else {
    if (checkbox.dataset.packagename in selectedApps) {  // App is selected
      delete selectedApps[checkbox.dataset.packagename];
    }
  }
}

function getSelectedPackages() {
  var selectedPackages = [];
  for (var packageName in selectedApps) {
    if (!selectedApps.hasOwnProperty(packageName)) {
      continue;
    }

    selectedPackages.push(packageName);
  }
  return selectedPackages;
}