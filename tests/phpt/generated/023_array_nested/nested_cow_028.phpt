--TEST--
nested array copy-on-write vector 028
--FILE--
<?php
$tree = ['node' => ['value' => 113, 'items' => [114]]];
$copy = $tree;
$copy['node']['value'] += 2;
$copy['node']['items'][] = 117;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
113:115:114:114,117
