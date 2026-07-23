--TEST--
object clone and property COW vector 012
--FILE--
<?php
class GeneratedClone012 { public $items; public function __construct($items) { $this->items = $items; } }
$original = new GeneratedClone012([15, 29]);
$copy = clone $original;
$copy->items[0] = 116;
echo implode(',', $original->items), ':', implode(',', $copy->items);
--EXPECT--
15,29:116,29
