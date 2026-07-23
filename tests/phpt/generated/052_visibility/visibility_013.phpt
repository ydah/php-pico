--TEST--
private and protected member access vector 013
--FILE--
<?php
class GeneratedVisibility013 {
    private $left = 95; protected $right = 71;
    private function left() { return $this->left; }
    protected function right() { return $this->right; }
    public function total() { return $this->left() + $this->right(); }
}
echo (new GeneratedVisibility013())->total();
--EXPECT--
166
