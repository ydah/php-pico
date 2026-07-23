--TEST--
nested array copy-on-write vector 020
--FILE--
<?php
$tree = ['node' => ['value' => 81, 'items' => [82]]];
$copy = $tree;
$copy['node']['value'] += 8;
$copy['node']['items'][] = 91;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
81:89:82:82,91
