--TEST--
nested array copy-on-write vector 026
--FILE--
<?php
$tree = ['node' => ['value' => 105, 'items' => [106]]];
$copy = $tree;
$copy['node']['value'] += 7;
$copy['node']['items'][] = 114;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
105:112:106:106,114
